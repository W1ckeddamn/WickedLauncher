#include <QTest>
#include <QFile>
#include <QPair>
#include <QTemporaryDir>

#include <algorithm>

#include "archive/ArchiveWriter.h"
#include "minecraft/mod/ModChecker.h"

// The class is declared before the raw string literals below: moc cannot parse
// raw strings and would lose the class body if they came first.
class ModCheckerTest : public QObject {
    Q_OBJECT

   private slots:
    void fabricManifest();
    void fabricMissingDependency();
    void fabricApiSatisfiesModules();
    void quiltManifest();
    void forgeToml();
    void legacyInfo();
    void duplicateIds();
    void knownConflict();
    void declaredConflict();
    void scanDirectory();
    void unreadableJarIsReported();
};

namespace {
bool makeJar(const QString& path, const QList<QPair<QString, QByteArray>>& files)
{
    MMCZip::ArchiveWriter writer(path);
    if (!writer.open()) {
        return false;
    }
    for (const auto& [name, data] : files) {
        if (!writer.addFile(name, data)) {
            return false;
        }
    }
    return static_cast<bool>(writer.close());
}

bool hasSummary(const QList<ModChecker::Issue>& issues, const QString& needle)
{
    return std::any_of(issues.begin(), issues.end(), [&needle](const auto& issue) { return issue.summary.contains(needle); });
}

int countSeverity(const QList<ModChecker::Issue>& issues, ModChecker::Issue::Severity severity)
{
    return static_cast<int>(
        std::count_if(issues.begin(), issues.end(), [severity](const auto& issue) { return issue.severity == severity; }));
}

const QByteArray s_fabricA = R"({
    "schemaVersion": 1,
    "id": "mod_a",
    "name": "Mod A",
    "version": "1.2.3",
    "depends": { "minecraft": ">=1.20", "mod_b": ">=2.0", "fabric-api-base": "*" },
    "provides": [ "mod_a_api" ],
    "breaks": { "bad_mod": "<1.0" }
})";
}  // namespace

void ModCheckerTest::fabricManifest()
{
    const auto info = ModChecker::parseFabric(s_fabricA, QStringLiteral("a.jar"));
    QVERIFY(info.error.isEmpty());
    QCOMPARE(info.id, QStringLiteral("mod_a"));
    QCOMPARE(info.name, QStringLiteral("Mod A"));
    QCOMPARE(info.version, QStringLiteral("1.2.3"));
    QVERIFY(info.required.contains(QStringLiteral("mod_b")));
    QVERIFY(info.required.contains(QStringLiteral("fabric-api-base")));
    QVERIFY(info.provides.contains(QStringLiteral("mod_a_api")));
    QVERIFY(info.conflicts.contains(QStringLiteral("bad_mod")));
    // Platform entries are filtered later; parsing keeps them so callers can decide.
    QVERIFY(info.required.contains(QStringLiteral("minecraft")));
}

void ModCheckerTest::fabricMissingDependency()
{
    const auto a = ModChecker::parseFabric(s_fabricA, QStringLiteral("a.jar"));
    const QByteArray b = R"({ "schemaVersion": 1, "id": "mod_b", "version": "2.1" })";
    auto issues = ModChecker::analyze({ a, ModChecker::parseFabric(b, QStringLiteral("b.jar")) });
    // fabric-api-base is required but no mod provides it: exactly one grouped warning.
    QVERIFY(hasSummary(issues, QStringLiteral("Fabric API")));
    QVERIFY(!hasSummary(issues, QStringLiteral("mod_b")));

    issues = ModChecker::analyze({ a });
    QVERIFY(hasSummary(issues, QStringLiteral("mod_b")));
}

void ModCheckerTest::fabricApiSatisfiesModules()
{
    const auto a = ModChecker::parseFabric(s_fabricA, QStringLiteral("a.jar"));
    const QByteArray b = R"({ "schemaVersion": 1, "id": "mod_b", "version": "2.1" })";
    const QByteArray api = R"({ "schemaVersion": 1, "id": "fabric-api", "version": "0.97.0" })";
    const auto issues = ModChecker::analyze({ a, ModChecker::parseFabric(b, QStringLiteral("b.jar")),
                                              ModChecker::parseFabric(api, QStringLiteral("fabric-api.jar")) });
    // The modules are nested inside the Fabric API jar and never appear as separate mods.
    QVERIFY(!hasSummary(issues, QStringLiteral("Fabric API")));
}

void ModCheckerTest::quiltManifest()
{
    const QByteArray data = R"({
        "schema_version": 1,
        "quilt_loader": {
            "id": "quilt_mod",
            "version": "0.5",
            "metadata": { "name": "Quilt Mod" },
            "provides": [ { "id": "quilt_api" } ],
            "depends": [ { "id": "minecraft" }, { "id": "qdep" } ],
            "breaks": [ { "id": "old_mod" } ]
        }
    })";
    const auto info = ModChecker::parseQuilt(data, QStringLiteral("q.jar"));
    QCOMPARE(info.id, QStringLiteral("quilt_mod"));
    QCOMPARE(info.name, QStringLiteral("Quilt Mod"));
    QVERIFY(info.required.contains(QStringLiteral("qdep")));
    QVERIFY(info.provides.contains(QStringLiteral("quilt_api")));
    QVERIFY(info.conflicts.contains(QStringLiteral("old_mod")));
}

void ModCheckerTest::forgeToml()
{
    const QByteArray data = R"toml(
modLoader = "javafml"
loaderVersion = "[47,)"
license = "MIT"

[[mods]]
modId = "forge_mod"
version = "3.0"
displayName = "Forge Mod"

[[dependencies.forge_mod]]
modId = "minecraft"
mandatory = true

[[dependencies.forge_mod]]
modId = "other_mod"
mandatory = true

[[dependencies.forge_mod]]
modId = "optional_mod"
mandatory = false
type = "optional"

[[dependencies.forge_mod]]
modId = "enemy_mod"
mandatory = false
type = "incompatible"
)toml";
    const auto info = ModChecker::parseForgeToml(data, QStringLiteral("f.jar"));
    QVERIFY2(info.error.isEmpty(), qPrintable(info.error));
    QCOMPARE(info.id, QStringLiteral("forge_mod"));
    QCOMPARE(info.version, QStringLiteral("3.0"));
    QVERIFY(info.required.contains(QStringLiteral("other_mod")));
    QVERIFY(!info.required.contains(QStringLiteral("optional_mod")));
    // Platform ids are kept by the parser; analyze() filters them out.
    QVERIFY(info.required.contains(QStringLiteral("minecraft")));
    QVERIFY(info.conflicts.contains(QStringLiteral("enemy_mod")));
}

void ModCheckerTest::legacyInfo()
{
    const QByteArray data = R"([{
        "modid": "mod_legacy",
        "name": "Legacy",
        "version": "1.0",
        "requiredMods": [ "mod_dep@1.0", "Forge" ]
    }])";
    const auto info = ModChecker::parseLegacyInfo(data, QStringLiteral("l.jar"));
    QCOMPARE(info.id, QStringLiteral("legacy"));
    QVERIFY(info.required.contains(QStringLiteral("dep")));
    // "Forge" is a platform dependency; analyze() ignores it.
    QVERIFY(info.required.contains(QStringLiteral("forge")));
}

void ModCheckerTest::duplicateIds()
{
    const QByteArray first = R"({ "schemaVersion": 1, "id": "dupe", "version": "1.0" })";
    const QByteArray second = R"({ "schemaVersion": 1, "id": "dupe", "version": "2.0" })";
    const auto issues = ModChecker::analyze(
        { ModChecker::parseFabric(first, QStringLiteral("dupe-1.jar")),
          ModChecker::parseFabric(second, QStringLiteral("dupe-2.jar")) });
    QVERIFY(hasSummary(issues, QStringLiteral("Duplicate mod")));
    QCOMPARE(countSeverity(issues, ModChecker::Issue::Severity::Error), 1);
}

void ModCheckerTest::knownConflict()
{
    const QByteArray optifine = R"({ "schemaVersion": 1, "id": "optifine", "version": "1.0" })";
    const QByteArray sodium = R"({ "schemaVersion": 1, "id": "sodium", "version": "0.5" })";
    const auto issues = ModChecker::analyze(
        { ModChecker::parseFabric(optifine, QStringLiteral("optifine.jar")),
          ModChecker::parseFabric(sodium, QStringLiteral("sodium.jar")) });
    QVERIFY(hasSummary(issues, QStringLiteral("Incompatible mods")));
    QCOMPARE(countSeverity(issues, ModChecker::Issue::Severity::Error), 1);
}

void ModCheckerTest::declaredConflict()
{
    const auto a = ModChecker::parseFabric(s_fabricA, QStringLiteral("a.jar"));
    const QByteArray bad = R"({ "schemaVersion": 1, "id": "bad_mod", "version": "0.5" })";
    const auto issues = ModChecker::analyze({ a, ModChecker::parseFabric(bad, QStringLiteral("bad.jar")) });
    QVERIFY(hasSummary(issues, QStringLiteral("declares a conflict")));
}

void ModCheckerTest::scanDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QVERIFY(makeJar(dir.filePath(QStringLiteral("mod-a.jar")), { { QStringLiteral("fabric.mod.json"), s_fabricA } }));
    QVERIFY(makeJar(dir.filePath(QStringLiteral("mod-c.jar")),
                    { { QStringLiteral("META-INF/mods.toml"),
                        QByteArray("[[mods]]\nmodId=\"mod_c\"\nversion=\"1\"\n") } }));
    // Disabled resources must not take part in the check.
    QVERIFY(makeJar(dir.filePath(QStringLiteral("mod-d.jar.disabled")), { { QStringLiteral("fabric.mod.json"), s_fabricA } }));

    const auto mods = ModChecker::scan(dir.path());
    QCOMPARE(mods.size(), 2);
    QCOMPARE(mods.at(0).id, QStringLiteral("mod_a"));
    QCOMPARE(mods.at(1).id, QStringLiteral("mod_c"));
}

void ModCheckerTest::unreadableJarIsReported()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile broken(dir.filePath(QStringLiteral("broken.jar")));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("this is not a zip archive");
    broken.close();

    const auto issues = ModChecker::check(dir.path());
    QVERIFY(hasSummary(issues, QStringLiteral("Could not read mod metadata")));
}

QTEST_GUILESS_MAIN(ModCheckerTest)

#include "ModChecker_test.moc"
