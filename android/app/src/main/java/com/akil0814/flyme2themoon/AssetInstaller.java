package com.akil0814.flyme2themoon;

import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.function.IntConsumer;
import java.util.zip.*;

/** Android-independent installer; also exercised by the host JVM tests. */
public final class AssetInstaller {
    public record Entry(String path, long size, String sha256) {}
    public record Manifest(String hash, long totalBytes, List<Entry> files) {}
    public interface Source { InputStream open() throws IOException; }

    public static synchronized File install(File parent, Manifest manifest, Source source, IntConsumer progress) throws IOException {
        if (!manifest.hash().matches("[a-f0-9]{64}")) throw new IOException("Invalid resource version");
        Files.createDirectories(parent.toPath());
        File target = new File(parent, manifest.hash());
        File ready = new File(target, ".complete");
        if (ready.isFile() && new String(Files.readAllBytes(ready.toPath()), StandardCharsets.UTF_8).equals(manifest.hash()) && valid(target, manifest)) {
            progress.accept(100);
            cleanup(parent, target);
            return target;
        }
        File staging = new File(parent, manifest.hash() + ".partial");
        delete(staging);
        Files.createDirectories(staging.toPath());
        if (manifest.totalBytes() < 0 || manifest.totalBytes() > parent.getUsableSpace() - 16L * 1024 * 1024)
            throw new IOException("Not enough free storage to prepare game resources");
        Map<String, Entry> expected = new HashMap<>();
        for (Entry entry : manifest.files()) {
            safeFile(staging, entry.path());
            if (entry.size() < 0 || !entry.sha256().matches("[a-f0-9]{64}") || expected.put(entry.path(), entry) != null)
                throw new IOException("Invalid resource manifest");
        }
        long total = 0;
        try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(source.open()))) {
            ZipEntry zipped;
            byte[] buffer = new byte[65536];
            while ((zipped = zip.getNextEntry()) != null) {
                Entry entry = expected.remove(zipped.getName());
                if (entry == null || zipped.isDirectory()) throw new IOException("Unexpected resource: " + zipped.getName());
                File destination = safeFile(staging, entry.path());
                Files.createDirectories(destination.getParentFile().toPath());
                MessageDigest digest = sha256();
                long written = 0;
                try (FileOutputStream output = new FileOutputStream(destination)) {
                    int count;
                    while ((count = zip.read(buffer)) != -1) {
                        written += count;
                        if (written > entry.size()) throw new IOException("Resource size mismatch: " + entry.path());
                        output.write(buffer, 0, count);
                        digest.update(buffer, 0, count);
                    }
                    output.getFD().sync();
                }
                if (written != entry.size() || !hex(digest.digest()).equals(entry.sha256()))
                    throw new IOException("Resource checksum mismatch: " + entry.path());
                total += written;
                progress.accept((int)(total * 100 / Math.max(1, manifest.totalBytes())));
            }
        }
        if (!expected.isEmpty() || total != manifest.totalBytes()) throw new IOException("Incomplete game resources");
        Files.write(new File(staging, ".complete").toPath(), manifest.hash().getBytes(StandardCharsets.UTF_8));
        delete(target);
        Files.move(staging.toPath(), target.toPath(), StandardCopyOption.ATOMIC_MOVE);
        cleanup(parent, target);
        return target;
    }

    private static boolean valid(File root, Manifest manifest) throws IOException {
        // Avoid hashing 145 MiB on every launch; extraction checks hashes, reuse checks presence/size.
        for (Entry entry : manifest.files()) {
            File file = safeFile(root, entry.path());
            if (!file.isFile() || file.length() != entry.size()) return false;
        }
        return true;
    }
    private static File safeFile(File root, String relative) throws IOException {
        if (!relative.startsWith("assets/") || relative.contains("\\")) throw new IOException("Invalid resource path");
        File candidate = new File(root, relative).getCanonicalFile();
        if (!candidate.toPath().startsWith(root.getCanonicalFile().toPath())) throw new IOException("Resource path escaped its root");
        return candidate;
    }
    private static void cleanup(File parent, File keep) throws IOException {
        File[] children = parent.listFiles();
        if (children != null) for (File child : children)
            if (!child.equals(keep) && child.getName().matches("[a-f0-9]{64}(\\.partial)?")) delete(child);
    }
    static void delete(File file) throws IOException {
        if (!file.exists()) return;
        if (Files.isDirectory(file.toPath(), LinkOption.NOFOLLOW_LINKS)) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Cannot inspect " + file);
            for (File child : children) delete(child);
        }
        Files.delete(file.toPath());
    }
    private static MessageDigest sha256() { try { return MessageDigest.getInstance("SHA-256"); } catch (NoSuchAlgorithmException e) { throw new AssertionError(e); } }
    static String hex(byte[] data) { StringBuilder out = new StringBuilder(); for (byte b : data) out.append(String.format(Locale.ROOT,"%02x", b & 255)); return out.toString(); }
}
