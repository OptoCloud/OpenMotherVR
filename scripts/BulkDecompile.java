// Ghidra headless post-script: decompile every function to per-function .c files
// plus one concatenated all_functions.c master listing.
//
// Run via analyzeHeadless with: -postScript BulkDecompile.java -scriptPath <this dir>
// OUTDIR env var controls output root.
//
//@category MotherVR
//@menupath
//@keybinding
//@toolbar

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.util.task.ConsoleTaskMonitor;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class BulkDecompile extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String outEnv = System.getenv("OUTDIR");
        Path outRoot = (outEnv != null && !outEnv.isEmpty())
                ? Paths.get(outEnv)
                : Paths.get(getProjectRootFolder().getProjectLocator().getProjectDir().getAbsolutePath(), "dumps");
        Path perFn = outRoot.resolve("decompiled");
        Files.createDirectories(perFn);
        Path masterPath = outRoot.resolve("all_functions.c");

        DecompInterface decomp = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        decomp.setOptions(opts);
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(false);
        decomp.setSimplificationStyle("decompile");
        if (!decomp.openProgram(currentProgram)) {
            printerr("Failed to open program for decompile: " + decomp.getLastMessage());
            return;
        }

        FunctionManager fm = currentProgram.getFunctionManager();
        int total = fm.getFunctionCount();
        println("[BulkDecompile] " + total + " functions to decompile");

        int ok = 0, fail = 0, skip = 0;
        try (PrintWriter master = new PrintWriter(Files.newBufferedWriter(masterPath))) {
            for (Function f : fm.getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isThunk()) { skip++; continue; }

                String safe = sanitize(f.getName(true)) + "_" + f.getEntryPoint().toString();
                Path target = perFn.resolve(safe + ".c");

                try {
                    DecompileResults res = decomp.decompileFunction(f, 60, new ConsoleTaskMonitor());
                    if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                        String src = res.getDecompiledFunction().getC();
                        String header = "// " + f.getName(true) + " @ " + f.getEntryPoint()
                                + "  cc=" + f.getCallingConventionName()
                                + "  size=" + f.getBody().getNumAddresses() + "\n";
                        try (BufferedWriter bw = Files.newBufferedWriter(target)) {
                            bw.write(header);
                            bw.write(src);
                        }
                        master.write(header);
                        master.write(src);
                        master.write("\n");
                        ok++;
                    } else {
                        fail++;
                        String msg = res == null ? "null result" : res.getErrorMessage();
                        try (BufferedWriter bw = Files.newBufferedWriter(target)) {
                            bw.write("// DECOMPILE FAILED: " + msg + "\n");
                        }
                    }
                } catch (Exception ex) {
                    fail++;
                    printerr("decompile failed for " + f.getName() + ": " + ex);
                }

                if ((ok + fail) % 200 == 0) {
                    println("[BulkDecompile] progress " + (ok + fail) + "/" + total
                            + " ok=" + ok + " fail=" + fail + " skip=" + skip);
                }
            }
        }
        decomp.dispose();
        println("[BulkDecompile] done. ok=" + ok + " fail=" + fail + " skipped(thunk)=" + skip);
    }

    private static String sanitize(String name) {
        StringBuilder sb = new StringBuilder(name.length());
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (Character.isLetterOrDigit(c) || c == '_' || c == '.') sb.append(c);
            else sb.append('_');
        }
        String s = sb.toString();
        if (s.length() > 120) s = s.substring(0, 120);
        return s;
    }
}
