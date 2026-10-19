// Decompile functions containing the given addresses (hex), print C to stdout.
// Usage: -postScript Decompile.java [depth=N] 0x401000 0x402000 ...
//   depth=N also decompiles callees up to N levels deep (each function once).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.util.*;

public class Decompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        int depth = 0;
        Deque<Object[]> work = new ArrayDeque<>();
        for (String a : getScriptArgs()) {
            if (a.startsWith("depth=")) {
                depth = Integer.parseInt(a.substring(6));
                continue;
            }
            Function f = getFunctionContaining(toAddr(a));
            if (f == null) {
                // Ghidra missed it (e.g. only reached via vtable): create it in this transient session.
                disassemble(toAddr(a));
                f = createFunction(toAddr(a), null);
            }
            if (f == null) {
                println("=== no function at " + a);
                continue;
            }
            work.add(new Object[] {f, 0});
        }
        Set<Address> seen = new HashSet<>();
        while (!work.isEmpty()) {
            Object[] w = work.poll();
            Function f = (Function) w[0];
            int d = (Integer) w[1];
            if (!seen.add(f.getEntryPoint())) continue;
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            println("=== " + f.getName() + " @ " + f.getEntryPoint() + " (depth " + d + ")");
            println(r.getDecompiledFunction() != null ? r.getDecompiledFunction().getC() : r.getErrorMessage());
            if (d < depth) {
                for (Function c : f.getCalledFunctions(monitor)) {
                    if (!c.isThunk() && !c.isExternal() && c.getBody().getNumAddresses() > 16)
                        work.add(new Object[] {c, d + 1});
                }
            }
        }
    }
}
