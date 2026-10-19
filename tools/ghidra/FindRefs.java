// Find code referencing given scalar constants or strings.
// Usage: -postScript FindRefs.java scalar:0xE9612C00 str:musicSFX ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;
import ghidra.program.util.DefinedStringIterator;

public class FindRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        for (String arg : getScriptArgs()) {
            if (arg.startsWith("scalar:")) {
                long v = Long.decode(arg.substring(7)) & 0xffffffffL;
                println("=== scalar " + arg);
                for (Instruction ins : listing.getInstructions(true)) {
                    for (int i = 0; i < ins.getNumOperands(); i++) {
                        for (Object o : ins.getOpObjects(i)) {
                            if (o instanceof Scalar s && (s.getUnsignedValue() & 0xffffffffL) == v) {
                                Function f = getFunctionContaining(ins.getAddress());
                                println(ins.getAddress() + " " + (f != null ? f.getName() + "@" + f.getEntryPoint() : "?") + " : " + ins);
                            }
                        }
                    }
                }
            } else if (arg.startsWith("ref:")) {
                println("=== refs to " + arg.substring(4));
                for (Reference r : getReferencesTo(toAddr(arg.substring(4)))) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    println("   <- " + r.getFromAddress() + " " + r.getReferenceType() + " " + (f != null ? f.getName() + "@" + f.getEntryPoint() : "?"));
                }
            } else if (arg.startsWith("str:")) {
                String needle = arg.substring(4).toLowerCase();
                println("=== string " + needle);
                for (Data d : DefinedStringIterator.forProgram(currentProgram)) {
                    Object val = d.getValue();
                    if (val == null || !val.toString().toLowerCase().contains(needle)) continue;
                    println(d.getAddress() + " \"" + val + "\"");
                    for (Reference r : getReferencesTo(d.getAddress())) {
                        Function f = getFunctionContaining(r.getFromAddress());
                        println("   <- " + r.getFromAddress() + " " + (f != null ? f.getName() + "@" + f.getEntryPoint() : "?"));
                    }
                }
            }
        }
    }
}
