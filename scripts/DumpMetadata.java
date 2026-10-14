// Ghidra headless post-script: dump PE metadata to text files.
// Run via analyzeHeadless with: -postScript DumpMetadata.java -scriptPath <this dir>
//
// Output files are written to ${OUTDIR} (env var) or <project>/dumps/ as fallback.
//
//@category MotherVR
//@menupath
//@keybinding
//@toolbar

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.*;
import ghidra.program.model.util.CodeUnitInsertionException;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class DumpMetadata extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String outEnv = System.getenv("OUTDIR");
        Path outDir;
        if (outEnv != null && !outEnv.isEmpty()) {
            outDir = Paths.get(outEnv);
        } else {
            outDir = Paths.get(getProjectRootFolder().getProjectLocator().getProjectDir().getAbsolutePath(), "dumps");
        }
        Files.createDirectories(outDir);
        println("[DumpMetadata] writing to " + outDir);

        dumpSections(outDir);
        dumpExports(outDir);
        dumpImports(outDir);
        dumpFunctionsSummary(outDir);
        dumpStrings(outDir);

        println("[DumpMetadata] done");
    }

    private void dumpSections(Path outDir) throws IOException {
        try (PrintWriter pw = new PrintWriter(Files.newBufferedWriter(outDir.resolve("sections.tsv")))) {
            pw.println("name\tstart\tend\tsize\tread\twrite\texecute\tinitialized");
            for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
                pw.printf("%s\t%s\t%s\t%d\t%b\t%b\t%b\t%b%n",
                        b.getName(), b.getStart(), b.getEnd(), b.getSize(),
                        b.isRead(), b.isWrite(), b.isExecute(), b.isInitialized());
            }
        }
    }

    private void dumpExports(Path outDir) throws IOException {
        SymbolTable st = currentProgram.getSymbolTable();
        try (PrintWriter pw = new PrintWriter(Files.newBufferedWriter(outDir.resolve("exports.tsv")))) {
            pw.println("address\tname\tordinal");
            SymbolIterator it = st.getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (s.isExternalEntryPoint()) {
                    pw.printf("%s\t%s\t%s%n", s.getAddress(), s.getName(true), "");
                }
            }
        }
    }

    private void dumpImports(Path outDir) throws IOException {
        try (PrintWriter pw = new PrintWriter(Files.newBufferedWriter(outDir.resolve("imports.tsv")))) {
            pw.println("library\tsymbol\taddress");
            SymbolTable st = currentProgram.getSymbolTable();
            ExternalManager em = currentProgram.getExternalManager();
            for (String libName : em.getExternalLibraryNames()) {
                Iterator<ExternalLocation> it = em.getExternalLocations(libName);
                while (it.hasNext()) {
                    ExternalLocation el = it.next();
                    Address a = el.getAddress();
                    pw.printf("%s\t%s\t%s%n", libName, el.getLabel(), a == null ? "" : a.toString());
                }
            }
        }
    }

    private void dumpFunctionsSummary(Path outDir) throws IOException {
        try (PrintWriter pw = new PrintWriter(Files.newBufferedWriter(outDir.resolve("functions.tsv")))) {
            pw.println("address\tname\tsize\tparam_count\tcalling_convention\tis_thunk\tnamespace");
            FunctionManager fm = currentProgram.getFunctionManager();
            for (Function f : fm.getFunctions(true)) {
                pw.printf("%s\t%s\t%d\t%d\t%s\t%b\t%s%n",
                        f.getEntryPoint(),
                        f.getName(true),
                        f.getBody().getNumAddresses(),
                        f.getParameterCount(),
                        f.getCallingConventionName(),
                        f.isThunk(),
                        f.getParentNamespace().getName(true));
            }
        }
    }

    private void dumpStrings(Path outDir) throws IOException {
        try (PrintWriter pw = new PrintWriter(Files.newBufferedWriter(outDir.resolve("strings.tsv")))) {
            pw.println("address\tlength\ttype\tvalue");
            DataIterator di = currentProgram.getListing().getDefinedData(true);
            while (di.hasNext()) {
                Data d = di.next();
                if (d.hasStringValue()) {
                    StringDataInstance s = StringDataInstance.getStringDataInstance(d);
                    String val = s == null ? String.valueOf(d.getValue()) : s.getStringValue();
                    if (val == null) continue;
                    val = val.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
                    pw.printf("%s\t%d\t%s\t%s%n",
                            d.getAddress(),
                            d.getLength(),
                            d.getDataType().getName(),
                            val);
                }
            }
        }
    }
}
