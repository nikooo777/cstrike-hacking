import java.io.File;
import java.io.FileOutputStream;

import ghidra.app.script.GhidraScript;
import ghidra.program.database.mem.FileBytes;

public class ExportFileBytes extends GhidraScript {
    @Override
    protected void run() throws Exception {
        File outputDirectory = new File(getScriptArgs()[0]);
        String programPath = currentProgram.getDomainFile().getPathname().replace('/', '_');
        for (FileBytes fileBytes : currentProgram.getMemory().getAllFileBytes()) {
            byte[] bytes = new byte[(int) fileBytes.getSize()];
            fileBytes.getOriginalBytes(0, bytes);
            File output = new File(outputDirectory, programPath + "__" + new File(fileBytes.getFilename()).getName());
            try (FileOutputStream stream = new FileOutputStream(output)) {
                stream.write(bytes);
            }
            println("EXPORTED " + output + " size=" + bytes.length + " base=" + currentProgram.getImageBase());
        }
    }
}
