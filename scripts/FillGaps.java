// Ghidra headless post-script: find code that auto-analysis never turned into
// functions and create functions for it, so a subsequent BulkDecompile covers
// all of .text.
//
// Sources of missed code, in the order they are tried:
//   1. Relocation targets in .text. Every absolute pointer in a DLL has a
//      relocation entry, so this catches hook detours whose address is only
//      ever stored as data (e.g. "mov [patch_slot], offset LAB_10020c60").
//   2. Instructions that are not inside any function body (orphan code).
//   3. Undefined, non-padding byte runs in .text. Candidates from this source
//      that turn out to be alignment filler (multi-byte NOPs) or stray ret
//      tails are removed again, see isJunk().
// Repeats until a pass creates nothing new, then prints a coverage report.
//
// Run with the project saved (no -readOnly) so the new functions persist:
//   analyzeHeadless.bat <projectDir> mothervr -process dxgi.dll -noanalysis \
//     -scriptPath <scripts> -postScript FillGaps.java
//
//@category MotherVR

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.reloc.Relocation;

import java.util.*;

public class FillGaps extends GhidraScript {

    private MemoryBlock text;
    private Listing listing;
    private final List<String> created = new ArrayList<>();

    @Override
    protected void run() throws Exception {
        text = currentProgram.getMemory().getBlock(".text");
        listing = currentProgram.getListing();
        if (text == null) { printerr("no .text block"); return; }

        report("before");
        for (int pass = 1; pass <= 10; pass++) {
            int n = created.size();
            fromRelocations();
            fromOrphanInstructions();
            fromUndefinedRuns();
            analyzeChanges(currentProgram);
            println("[FillGaps] pass " + pass + ": +" + (created.size() - n) + " functions");
            if (created.size() == n) break;
        }
        for (String s : created) println("[FillGaps] created " + s);
        println("[FillGaps] discarded " + junk + " junk candidates (padding / stray ret)");
        report("after");
    }

    private boolean inText(Address a) { return a != null && text.contains(a); }

    private void fromRelocations() throws Exception {
        Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations();
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Set<Address> targets = new TreeSet<>();
        while (it.hasNext()) {
            Relocation r = it.next();
            try {
                long v = currentProgram.getMemory().getInt(r.getAddress()) & 0xffffffffL;
                Address t = space.getAddress(v);
                if (inText(t)) targets.add(t);
            } catch (MemoryAccessException | AddressOutOfBoundsException e) {
                // reloc site in uninitialized memory or a bogus value: ignore
            }
        }
        for (Address t : targets) tryCreate(t, "reloc");
    }

    private void fromOrphanInstructions() throws Exception {
        List<Address> starts = new ArrayList<>();
        Address prevEnd = null;
        for (Instruction ins : listing.getInstructions(new AddressSet(text.getStart(), text.getEnd()), true)) {
            if (getFunctionContaining(ins.getAddress()) != null) { prevEnd = null; continue; }
            // start of a contiguous orphan run
            if (prevEnd == null || !ins.getAddress().equals(prevEnd)) starts.add(ins.getAddress());
            prevEnd = ins.getMaxAddress().next();
        }
        for (Address a : starts) tryCreate(a, "orphan");
    }

    private void fromUndefinedRuns() throws Exception {
        Memory mem = currentProgram.getMemory();
        Address a = text.getStart();
        while (a != null && a.compareTo(text.getEnd()) <= 0) {
            CodeUnit cu = listing.getCodeUnitAt(a);
            boolean undefined = cu instanceof Data && !((Data) cu).isDefined();
            if (!undefined) {
                a = (cu != null ? cu.getMaxAddress() : a).next();
                continue;
            }
            byte b = mem.getByte(a);
            // MSVC inter-function padding
            if (b == (byte) 0xCC || b == (byte) 0x00 || b == (byte) 0x90) { a = a.next(); continue; }
            if (!tried.add(a)) { a = a.next(); continue; }
            tryCreate(a, "undefined");
            Function f = getFunctionAt(a);
            if (f != null && isJunk(f)) {
                // Alignment filler or a stray ret tail, not a real function.
                AddressSetView body = f.getBody();
                removeFunction(f);
                clearListing(body);
                created.remove(created.size() - 1);
                junk++;
                a = body.getMaxAddress().next();
                continue;
            }
            CodeUnit after = listing.getCodeUnitAt(a);
            a = (after != null ? after.getMaxAddress() : a).next();
        }
    }

    private final Set<Address> tried = new HashSet<>();
    private int junk = 0;

    // A function found only by disassembling unreferenced bytes is junk when
    // it is tiny or made of nothing but padding idioms, jumps and returns.
    private boolean isJunk(Function f) {
        if (f.getBody().getNumAddresses() <= 4) return true;
        for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
            String m = ins.getMnemonicString();
            if (!(isPaddingIdiom(ins) || m.equals("JMP") || m.equals("RET"))) return false;
        }
        return true;
    }

    // MSVC multi-byte NOPs: nop/nop dword ptr [...], mov r,r and lea r,[r+0].
    private boolean isPaddingIdiom(Instruction ins) {
        String m = ins.getMnemonicString();
        if (m.equals("NOP")) return true;
        if (ins.getNumOperands() != 2) return false;
        Object[] dst = ins.getOpObjects(0), src = ins.getOpObjects(1);
        if (dst.length != 1 || !(dst[0] instanceof ghidra.program.model.lang.Register)) return false;
        if (m.equals("MOV")) return src.length == 1 && dst[0].equals(src[0]);
        if (m.equals("LEA")) {
            boolean sameReg = false, zeroDisp = true;
            for (Object o : src) {
                if (o.equals(dst[0])) sameReg = true;
                else if (o instanceof ghidra.program.model.scalar.Scalar s) zeroDisp &= s.getValue() == 0;
                else return false;
            }
            return sameReg && zeroDisp;
        }
        return false;
    }

    private void tryCreate(Address a, String why) {
        if (getFunctionContaining(a) != null) return;
        if (listing.getInstructionAt(a) == null) {
            Data d = listing.getDataAt(a);
            if (d != null && d.isDefined()) return; // typed data (strings, tables): leave alone
            DisassembleCommand dis = new DisassembleCommand(a, null, true);
            if (!dis.applyTo(currentProgram, monitor) || listing.getInstructionAt(a) == null) return;
        }
        CreateFunctionCmd cmd = new CreateFunctionCmd(a);
        if (cmd.applyTo(currentProgram, monitor) && getFunctionAt(a) != null) {
            Function f = getFunctionAt(a);
            created.add(a + "\t" + why + "\t" + f.getBody().getNumAddresses() + " bytes");
        }
    }

    private void report(String label) throws Exception {
        Memory mem = currentProgram.getMemory();
        long inFn = 0, orphanIns = 0, undef = 0, pad = 0;
        for (Address a = text.getStart(); a != null && a.compareTo(text.getEnd()) <= 0; ) {
            CodeUnit cu = listing.getCodeUnitAt(a);
            long len = cu != null ? cu.getLength() : 1;
            if (getFunctionContaining(a) != null) inFn += len;
            else if (cu instanceof Instruction) orphanIns += len;
            else {
                byte b = mem.getByte(a);
                if (b == (byte) 0xCC || b == (byte) 0x00 || b == (byte) 0x90) pad += len; else undef += len;
            }
            a = cu != null ? cu.getMaxAddress().next() : a.next();
        }
        long total = text.getSize();
        println(String.format("[FillGaps] coverage %s: functions=%d (%.2f%%) orphan_code=%d undefined=%d padding=%d total=%d fn_count=%d",
                label, inFn, 100.0 * inFn / total, orphanIns, undef, pad, total,
                currentProgram.getFunctionManager().getFunctionCount()));
    }
}
