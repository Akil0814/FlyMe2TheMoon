package com.akil0814.flyme2themoon;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

public final class AssetInstallerTest {
    private static int assertions;
    static void check(boolean condition, String message) {
        ++assertions;
        if (!condition) throw new AssertionError(message);
    }
    static String hash(byte[] bytes) throws Exception { return AssetInstaller.hex(MessageDigest.getInstance("SHA-256").digest(bytes)); }
    static byte[] zip(String path, byte[] bytes) throws IOException {
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        try (ZipOutputStream stream = new ZipOutputStream(buffer)) {
            stream.putNextEntry(new ZipEntry(path)); stream.write(bytes); stream.closeEntry();
        }
        return buffer.toByteArray();
    }
    static AssetInstaller.Manifest manifest(String version, String path, byte[] bytes) throws Exception {
        return new AssetInstaller.Manifest(hash(version.getBytes(StandardCharsets.UTF_8)), bytes.length,
            List.of(new AssetInstaller.Entry(path, bytes.length, hash(bytes))));
    }
    interface Action { void run() throws Exception; }
    static void fails(Action action, String label) throws Exception {
        try { action.run(); throw new AssertionError("Expected failure: " + label); }
        catch (IOException expected) { ++assertions; }
    }
    public static void main(String[] args) throws Exception {
        File scratch = Files.createTempDirectory("fly-installer-test-").toFile();
        try {
            File content = new File(scratch,"content");
            byte[] data = "first version".getBytes(StandardCharsets.UTF_8);
            byte[] archive = zip("assets/data.json", data);
            var first = manifest("v1","assets/data.json",data);
            File installed = AssetInstaller.install(content,first,() -> new ByteArrayInputStream(archive),value -> {});
            check(Arrays.equals(Files.readAllBytes(new File(installed,"assets/data.json").toPath()),data),"First extraction");
            check(new File(installed,".complete").isFile(),"Completion marker");
            check(AssetInstaller.install(content,first,() -> { throw new AssertionError("Reopened unchanged pack"); },value -> {}).equals(installed),"Reuse unchanged resources");
            Files.delete(new File(installed,"assets/data.json").toPath());
            AssetInstaller.install(content,first,() -> new ByteArrayInputStream(archive),value -> {});
            check(new File(installed,"assets/data.json").isFile(),"Repair missing file");
            File saves = new File(scratch,"runtime/player_data/saves"); saves.mkdirs();
            Files.writeString(new File(saves,"progress.json").toPath(),"keep me");
            byte[] next = "new version".getBytes(StandardCharsets.UTF_8);
            var second = manifest("v2","assets/data.json",next);
            fails(() -> AssetInstaller.install(content,second,() -> new ByteArrayInputStream(archive),value -> {}),"Checksum mismatch");
            check(installed.isDirectory(),"Failed update preserves old resources");
            byte[] secondZip = zip("assets/data.json",next);
            File updated = AssetInstaller.install(content,second,() -> new ByteArrayInputStream(secondZip),value -> {});
            check(!installed.exists() && updated.isDirectory(),"Successful update removes old resources and partial directory");
            check(Files.readString(new File(saves,"progress.json").toPath()).equals("keep me"),"Update preserves save data");
            var third = manifest("v3","assets/data.json",next);
            fails(() -> AssetInstaller.install(content,third,() -> new InputStream() { public int read() throws IOException { throw new IOException("interrupted"); } },value -> {}),"Interrupted extraction");
            check(updated.isDirectory(),"Interrupted update preserves valid version");
            check(AssetInstaller.install(content,third,() -> new ByteArrayInputStream(secondZip),value -> {}).isDirectory(),"Retry after interruption");
            var missing = manifest("missing","assets/missing.json",next);
            fails(() -> AssetInstaller.install(content,missing,() -> new ByteArrayInputStream(new byte[0]),value -> {}),"Missing ZIP entry");
            var traversal = manifest("traversal","assets/../../escape",next);
            fails(() -> AssetInstaller.install(content,traversal,() -> new ByteArrayInputStream(zip("assets/../../escape",next)),value -> {}),"Traversal");
            var huge = new AssetInstaller.Manifest(hash("huge".getBytes()),Long.MAX_VALUE / 2,third.files());
            fails(() -> AssetInstaller.install(content,huge,() -> new ByteArrayInputStream(secondZip),value -> {}),"Insufficient storage");
            var duplicate = new AssetInstaller.Manifest(hash("duplicate".getBytes()),next.length*2,List.of(third.files().get(0),third.files().get(0)));
            fails(() -> AssetInstaller.install(content,duplicate,() -> new ByteArrayInputStream(secondZip),value -> {}),"Duplicate manifest path");
            System.out.println("AssetInstaller: " + assertions + " assertions passed");
        } finally { AssetInstaller.delete(scratch); }
    }
}
