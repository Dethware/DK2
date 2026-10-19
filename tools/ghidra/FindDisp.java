// List functions whose instructions use ALL of the given memory displacements and/or scalars.
// Usage: -postScript FindDisp.java +0x26 +0x6 +0x7 scalar:0x343fd
//   "+X"        operand text contains "+ X]"  (memory displacement)
//   "scalar:X"  any scalar operand equal to X
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class FindDisp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            boolean[] hit = new boolean[args.length];
            for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                for (int i = 0; i < ins.getNumOperands(); i++) {
                    String rep = ins.getDefaultOperandRepresentation(i);
                    for (int k = 0; k < args.length; k++) {
                        String a = args[k];
                        if (a.startsWith("+")) {
                            if (rep.contains("+ 0x" + Long.toHexString(Long.decode(a.substring(1))) + "]")) hit[k] = true;
                        } else if (a.startsWith("scalar:")) {
                            long v = Long.decode(a.substring(7));
                            for (Object o : ins.getOpObjects(i))
                                if (o instanceof Scalar s && s.getUnsignedValue() == v) hit[k] = true;
                        }
                    }
                }
            }
            boolean all = true;
            for (boolean h : hit) all &= h;
            if (all) println(f.getEntryPoint() + " " + f.getName() + " size=" + f.getBody().getNumAddresses());
        }
    }
}
