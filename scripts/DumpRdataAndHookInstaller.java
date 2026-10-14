// Dump .rdata bytes in the patch-payload region (0x1005dc00..0x1005de00)
// and the full disassembly listing of FUN_1001d6f0 @ 0x1001d6f0.
//
//@category MotherVR
//@menupath
//@keybinding
//@toolbar

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.lang.Register;

import java.io.*;
import java.nio.file.*;

public class DumpRdataAndHookInstaller extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String outEnv = System.getenv("OUTDIR");
        Path outDir = outEnv != null && !outEnv.isEmpty()
                ? Paths.get(outEnv)
                : Paths.get(getProjectRootFolder().getProjectLocator().getProjectDir().getAbsolutePath(), "dumps");
        Files.createDirectories(outDir);

        // 1. .rdata payload region — covers the 0x1005dcXX globals referenced
        //    by FUN_1001d6f0. Buffer is generous to capture neighbors.
        dumpBytesRange(outDir.resolve("rdata_payloads.txt"),
                0x1005dc00L, 0x1005de00L);

        // 2. Full disassembly listing of FUN_1001d6f0 with operand resolution.
        //    Shows the real patch sites and payload-global references that
        //    the decompiler obscures by inlining the Protect/Write/Restore.
        dumpInstructionListing(outDir.resolve("FUN_1001d6f0_listing.txt"),
                0x1001d6f0L, 0x1001e6deL);  // 4142 bytes => end at 0x1001e6de

        println("[DumpRdataAndHookInstaller] done");
    }

    private void dumpBytesRange(Path out, long startVa, long endVa) throws IOException {
        Memory mem = currentProgram.getMemory();
        Address start = toAddr(startVa);
        Address end = toAddr(endVa);
        try (PrintWriter pw = new PrintWriter(Files.newBufferedWriter(out))) {
            pw.printf("# bytes from %s to %s%n", start, end);
            long pos = startVa;
            while (pos < endVa) {
                long lineEnd = Math.min(pos + 16, endVa);
                StringBuilder hex = new StringBuilder();
                StringBuilder asc = new StringBuilder();
                for (long p = pos; p < lineEnd; p++) {
                    try {
                        byte b = mem.getByte(toAddr(p));
                        hex.append(String.format("%02x ", b & 0xff));
                        char c = (char) (b & 0xff);
                        asc.append((c >= 0x20 && c < 0x7f) ? c : '.');
                    } catch (Exception e) {
                        hex.append("?? ");
                        asc.append('.');
                    }
                }
                while (hex.length() < 48) hex.append(' ');
                pw.printf("%08x  %s |%s|%n", pos, hex, asc);
                pos = lineEnd;
            }
        }
    }

    private void dumpInstructionListing(Path out, long startVa, long endVa) throws IOException {
        Listing listing = currentProgram.getListing();
        try (PrintWriter pw = new PrintWriter(Files.newBufferedWriter(out))) {
            pw.printf("# instruction listing %08x .. %08x%n", startVa, endVa);
            Address a = toAddr(startVa);
            Address last = toAddr(endVa);
            while (a.compareTo(last) < 0) {
                CodeUnit cu = listing.getCodeUnitAt(a);
                if (cu == null) {
                    a = a.next();
                    if (a == null) break;
                    continue;
                }
                // Bytes column (up to 12 bytes shown; longer instructions get truncated)
                StringBuilder bytes = new StringBuilder();
                try {
                    byte[] bs = cu.getBytes();
                    for (int i = 0; i < bs.length && i < 12; i++) {
                        bytes.append(String.format("%02x", bs[i] & 0xff));
                    }
                } catch (Exception e) { /* ignore */ }
                while (bytes.length() < 24) bytes.append(' ');
                pw.printf("%s  %s  %s%n", cu.getAddress(), bytes, cu.toString());
                Address next = cu.getMaxAddress().next();
                if (next == null || next.compareTo(a) <= 0) break;
                a = next;
            }
        }
    }
}
