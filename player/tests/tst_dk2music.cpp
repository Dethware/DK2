#include "clipcache.h"
#include "gamedata.h"
#include "layer.h"
#include "mixer.h"
#include "musiclevel.h"
#include "sequencer.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QSet>
#include <QTest>
#include <QTimer>

#include <cmath>
#include <map>

using namespace dk2;

namespace {

SoundItem item(QList<SoundLink> links)
{
    SoundItem result;
    result.links = std::move(links);
    return result;
}

// Item 0 is the entry point to 1 (calm) or 3 (intense); 1 and 2 loop and jump to 3 when intense;
// 3 loops only while intense
QList<SoundItem> music()
{
    return {
        item({{1, 20, 39}, {3, 40, 100}}),
        item({{2, 20, 39}, {3, 40, 100}}),
        item({{1, 20, 39}, {3, 40, 100}}),
        item({{3, 40, 100}}),
    };
}

} // namespace

class TestDk2Music : public QObject {
    Q_OBJECT

private slots:
    void startsFromTheLinksOfTheFirstItem();
    void followsLinksMatchingTheIntensity();
    void staysSilentWithoutMatchAndResumesFromTheFirstItem();
    void repeatedLinksWeighMore();
    void unlinkedGroupsUseStartDelays();
    void levelForThreat();
    void levelDecaysToOne();
    void mixerResamples();

    void gameDataMaps();
    void gameDataRender();

private:
    QString gameDir() const;
};

void TestDk2Music::startsFromTheLinksOfTheFirstItem()
{
    const QList<SoundItem> items = music();
    QCOMPARE(Sequencer(items, 1).next(20), 1);
    QCOMPARE(Sequencer(items, 1).next(80), 3);
}

void TestDk2Music::followsLinksMatchingTheIntensity()
{
    const QList<SoundItem> items = music();
    Sequencer sequencer(items, 1);
    QCOMPARE(sequencer.next(20), 1);
    QCOMPARE(sequencer.next(39), 2);
    QCOMPARE(sequencer.next(20), 1);
    QCOMPARE(sequencer.next(40), 3);
    QCOMPARE(sequencer.next(100), 3);
}

void TestDk2Music::staysSilentWithoutMatchAndResumesFromTheFirstItem()
{
    const QList<SoundItem> items = music();
    Sequencer sequencer(items, 1);
    QCOMPARE(sequencer.next(0), -1);
    QCOMPARE(sequencer.next(60), 3);
    QCOMPARE(sequencer.next(20), -1);
    QCOMPARE(sequencer.next(19), -1);
    QCOMPARE(sequencer.next(20), 1);
}

void TestDk2Music::repeatedLinksWeighMore()
{
    const QList<SoundItem> items = {item({{1, 0, 100}, {2, 0, 100}, {1, 0, 100}}), item({}), item({})};
    std::map<int, int> counts;
    for (quint32 seed = 0; seed < 30000; ++seed)
        ++counts[Sequencer(items, seed).next(50)];
    QVERIFY(std::abs(counts[1] - 20000) < 600);
    QVERIFY(std::abs(counts[2] - 10000) < 600);
}

void TestDk2Music::unlinkedGroupsUseStartDelays()
{
    SoundItem atmos;
    atmos.minVolume = atmos.maxVolume = 21;
    atmos.minDelayMs = 8000;
    atmos.maxDelayMs = 20000;
    const QList<SoundItem> items = {atmos};
    Sequencer sequencer(items, 3);
    QVERIFY(!sequencer.isLinked());
    QCOMPARE(sequencer.next(0), 0);
    for (int i = 0; i < 1000; ++i) {
        const int delay = sequencer.pickStartDelayMs(atmos);
        QVERIFY(delay >= 8250 && delay < 20250);
    }
    QCOMPARE(sequencer.pickVolume(atmos), 0.21f);
}

void TestDk2Music::levelForThreat()
{
    const QList<int> thresholds = {100, 500, 1500};
    QCOMPARE(MusicLevel::levelForThreat(0, thresholds), int(MusicLevel::Silent));
    QCOMPARE(MusicLevel::levelForThreat(1, thresholds), int(MusicLevel::One));
    QCOMPARE(MusicLevel::levelForThreat(100, thresholds), int(MusicLevel::Two));
    QCOMPARE(MusicLevel::levelForThreat(499, thresholds), int(MusicLevel::Two));
    QCOMPARE(MusicLevel::levelForThreat(500, thresholds), int(MusicLevel::Three));
    QCOMPARE(MusicLevel::levelForThreat(1500, thresholds), int(MusicLevel::Four));
    QCOMPARE(MusicLevel::levelForThreat(99999, thresholds), int(MusicLevel::Four));
}

void TestDk2Music::levelDecaysToOne()
{
    MusicLevel level;
    level.update();
    QCOMPARE(level.intensity(), 0);
    level.raise(MusicLevel::Three);
    level.raise(MusicLevel::Two);
    level.update();
    QCOMPARE(level.intensity(), 60);
    level.update();
    QCOMPARE(level.intensity(), 20);
    level.set(MusicLevel::Silent);
    level.update();
    QCOMPARE(level.intensity(), 0);
}

void TestDk2Music::mixerResamples()
{
    // No layers: silence in any format, and the right amount of it
    Mixer mixer;
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    mixer.setOutputFormat(format);
    QVERIFY(mixer.open(QIODevice::ReadOnly));
    QByteArray data(48000 * format.bytesPerFrame(), char(1));
    QCOMPARE(mixer.read(data.data(), data.size()), qint64(data.size()));
    QVERIFY(std::all_of(data.cbegin(), data.cend(), [](char c) { return c == 0; }));
}

QString TestDk2Music::gameDir() const
{
    return qEnvironmentVariable("DK2_GAME_DIR");
}

void TestDk2Music::gameDataMaps()
{
    if (gameDir().isEmpty())
        QSKIP("Set DK2_GAME_DIR to a Dungeon Keeper II folder");

    GameData data;
    QString error;
    QVERIFY2(data.open(gameDir(), &error), qPrintable(error));

    const auto track1 = data.loadGroup(QStringLiteral("music"), 343, &error);
    QVERIFY2(track1, qPrintable(error));
    QCOMPARE(track1->group.items.size(), 135);
    QVERIFY(track1->group.isLinked());
    QCOMPARE(track1->group.flags, 0x0606);
    const auto track3 = data.loadGroup(QStringLiteral("music"), 345, &error);
    QVERIFY2(track3, qPrintable(error));
    QCOMPARE(track3->group.items.size(), 156);

    const auto ambience = data.loadGroup(QStringLiteral("ambience"), 341, &error);
    QVERIFY2(ambience, qPrintable(error));
    QCOMPARE(ambience->group.items.size(), 21);
    QCOMPARE(ambience->group.items.first().minVolume, 40);

    const auto atmos = data.loadGroup(QStringLiteral("one_shot_atmos"), 746, &error);
    QVERIFY2(atmos, qPrintable(error));
    QCOMPARE(atmos->group.items.size(), 1);
    QCOMPARE(atmos->group.items.first().sounds.size(), 19);
    QCOMPARE(atmos->group.items.first().minDelayMs, 8000);
    QCOMPARE(atmos->group.items.first().maxDelayMs, 20000);
    QVERIFY(!atmos->group.isLinked());

    // Every sound resolves to an archive entry with an exact PCM length
    for (const SoundItem &i : track1->group.items) {
        for (const SoundRef &sound : i.sounds) {
            const SdtEntry *entry = data.entry(track1->clip(sound));
            QVERIFY(entry);
            QVERIFY(entry->pcmBytes > 0);
        }
    }

    QCOMPARE(data.threatThresholds(), (QList<int>{100, 500, 1500}));
}

void TestDk2Music::gameDataRender()
{
    if (gameDir().isEmpty())
        QSKIP("Set DK2_GAME_DIR to a Dungeon Keeper II folder");

    GameData data;
    QVERIFY(data.open(gameDir()));
    const auto track1 = data.loadGroup(QStringLiteral("music"), 343);
    QVERIFY(track1);

    // Decode everything up front so the render below never has to wait
    ClipCache cache(&data);
    QSet<Clip> clips;
    for (const SoundItem &i : track1->group.items) {
        for (const SoundRef &sound : i.sounds)
            clips.insert(track1->clip(sound));
    }
    QEventLoop loop;
    qsizetype remaining = clips.size();
    connect(&cache, &ClipCache::decoded, &loop, [&] {
        if (--remaining == 0)
            loop.quit();
    });
    connect(&cache, &ClipCache::decodeFailed, &loop, [&] { loop.quit(); });
    QElapsedTimer timer;
    timer.start();
    for (const Clip &clip : std::as_const(clips))
        cache.request(clip);
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec();
    QCOMPARE(remaining, 0);
    qInfo("Decoded %lld clips in %lld ms", qlonglong(clips.size()), qlonglong(timer.elapsed()));

    std::atomic<int> intensity = 0;
    auto layer = std::make_shared<Layer>(*track1, &cache, [&intensity] { return intensity.load(); }, 7);
    struct Start {
        qint64 frame;
        int item;
        int sound;
    };
    QList<Start> started;
    qint64 frame = 0;
    layer->setStartedCallback([&](int item, int sound) { started.append({frame, item, sound}); });
    Mixer mixer;
    mixer.setLayers({layer});

    // 10 s silent, then a fight (20) for 40 s, then intense (60) for 40 s
    constexpr int Chunk = Layer::SampleRate / 10;
    QList<float> out;
    for (int second = 0; second < 90; ++second) {
        intensity = second < 10 ? 0 : second < 50 ? 20 : 60;
        for (int c = 0; c < 10; ++c) {
            const qsizetype start = out.size();
            out.resize(start + Chunk * 2);
            mixer.renderFrames(out.data() + start, Chunk);
            frame += Chunk;
        }
    }

    // Silent until the fight, then the music enters at the next retry
    QVERIFY(!started.isEmpty());
    QCOMPARE(started.first().frame, qint64(10 * Layer::SampleRate));
    QCOMPARE(started.first().item, 1); // track 1 always enters at item 2
    for (qsizetype i = 0; i < 10 * Layer::SampleRate * 2; ++i)
        QCOMPARE(out.at(i), 0.0f);

    // Every transition follows a link of the previous item, back to back without gaps.
    // The callback only knows the render chunk, so the exact joins come from the clip lengths.
    const QList<SoundItem> &items = track1->group.items;
    QList<qint64> joins = {started.first().frame};
    for (qsizetype i = 1; i < started.size(); ++i) {
        const Start &previous = started.at(i - 1);
        const int from = previous.item;
        const int to = started.at(i).item;
        const qint64 at = started.at(i).frame;
        const int level = at < 50 * Layer::SampleRate ? 20 : 60;
        const bool linked = std::any_of(items.at(from).links.cbegin(), items.at(from).links.cend(),
                                        [&](const SoundLink &l) { return l.target == to && l.matches(level); });
        QVERIFY2(linked, qPrintable(QStringLiteral("%1 -> %2").arg(from + 1).arg(to + 1)));
        const SdtEntry *entry = data.entry(track1->clip(items.at(from).sounds.at(previous.sound)));
        joins.append(joins.last() + entry->pcmBytes / 4);
        QVERIFY(at <= joins.last() && joins.last() < at + Chunk);
    }

    // Segment joins are as smooth as the music elsewhere: compare second differences
    auto curvature = [&out](qint64 f) {
        return std::abs(out.at((f + 1) * 2) - 2 * out.at(f * 2) + out.at((f - 1) * 2));
    };
    double atJoins = 0;
    double elsewhere = 0;
    for (qsizetype i = 1; i < joins.size(); ++i) {
        atJoins += curvature(joins.at(i));
        elsewhere += curvature(joins.at(i) - 5000);
    }
    QVERIFY2(atJoins < elsewhere * 4 + 0.01, qPrintable(QStringLiteral("joins %1 vs %2").arg(atJoins).arg(elsewhere)));
}

QTEST_GUILESS_MAIN(TestDk2Music)
#include "tst_dk2music.moc"
