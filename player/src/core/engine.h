#pragma once

#include "clipcache.h"
#include "gamedata.h"
#include "layer.h"
#include "mixer.h"
#include "musiclevel.h"

#include <QObject>
#include <QTimer>

#include <array>
#include <memory>

class QAudioSink;

namespace dk2 {

// The DK II background sound: music track, ambience and one shot atmosphere, driven by the music
// level like in the game. The UI only feeds it the battle threat; all the logic lives here.
class Engine : public QObject {
    Q_OBJECT
public:
    enum LayerId { Music, Ambience, Atmosphere, LayerCount };
    Q_ENUM(LayerId)

    // Music groups of the level sound tracks (the game's track 2, group 344, does not exist)
    static constexpr int MusicTrack1 = 343;
    static constexpr int MusicTrack3 = 345;
    static constexpr int AmbienceGroup = 341;
    static constexpr int AtmosphereGroup = 746;

    // The game updates the level once per game tick
    static constexpr int TickMs = 50;

    explicit Engine(QObject *parent = nullptr);
    ~Engine() override;

    bool open(const QString &gameDir);
    bool isOpen() const { return m_open; }

    // Start the background of a level with the given music group, silent like at level start
    bool start(int musicGroup);
    void stop();
    bool isPlaying() const { return m_sink != nullptr; }

    // The summed threat of the enemies the player fights, 0 when there is no fight.
    // Requested every tick, like an ongoing battle.
    void setThreat(int threat) { m_threat = threat; }
    int threat() const { return m_threat; }

    // Back to silence, as at level start
    void resetLevel() { m_level.set(MusicLevel::Silent); }

    int level() const { return m_publishedLevel; }
    int intensity() const { return m_level.intensity(); }
    QList<int> thresholds() const { return m_thresholds; }

    void setLayerGain(LayerId layer, float gain);
    void setMasterGain(float gain) { m_mixer.setMasterGain(gain); }

signals:
    void levelChanged(int level, int intensity);
    void itemStarted(dk2::Engine::LayerId layer, int item, const QString &description);
    void errorOccurred(const QString &message);

private:
    void tick();
    std::shared_ptr<Layer> createLayer(LayerId id, const QString &category, int group);
    QString describe(LayerId layer, int item, int sound) const;

    GameData m_data;
    ClipCache m_cache;
    Mixer m_mixer;
    MusicLevel m_level;
    QTimer m_ticker;
    QAudioSink *m_sink = nullptr;
    std::array<std::shared_ptr<Layer>, LayerCount> m_layers;
    std::array<float, LayerCount> m_gains{1.0f, 1.0f, 1.0f};
    QList<int> m_thresholds;
    int m_threat = 0;
    int m_publishedLevel = MusicLevel::Silent;
    bool m_open = false;
};

} // namespace dk2
