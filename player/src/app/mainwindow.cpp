#include "mainwindow.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

namespace {

constexpr int MaxThreat = 2000;
const auto GameDirKey = QStringLiteral("gameDir");

QSlider *volumeSlider(QWidget *parent)
{
    auto *slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(0, 100);
    slider->setValue(100);
    return slider;
}

} // namespace

MainWindow::MainWindow(const QString &gameDir, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(tr("Dungeon Keeper II music"));
    m_clock.start();

    // Game folder
    m_gameDir = new QLineEdit(gameDir.isEmpty() ? QSettings().value(GameDirKey).toString() : gameDir, this);
    auto *browseButton = new QPushButton(tr("Browse…"), this);
    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(m_gameDir);
    folderRow->addWidget(browseButton);

    // Playback
    m_track = new QComboBox(this);
    m_track->addItem(tr("Track 1"), dk2::Engine::MusicTrack1);
    m_track->addItem(tr("Track 3"), dk2::Engine::MusicTrack3);
    m_play = new QPushButton(tr("Play"), this);
    auto *playRow = new QHBoxLayout;
    playRow->addWidget(m_track, 1);
    playRow->addWidget(m_play);

    // Battle threat
    m_threat = new QSlider(Qt::Horizontal, this);
    m_threat->setRange(0, MaxThreat);
    m_threat->setSingleStep(10);
    m_threat->setPageStep(100);
    m_threatLabel = new QLabel(this);
    auto *resetButton = new QPushButton(tr("Reset to silent"), this);
    resetButton->setToolTip(tr("Back to the level start state, the music stays silent until the next fight"));
    m_levelLabel = new QLabel(this);
    auto *threatRow = new QHBoxLayout;
    threatRow->addWidget(m_threat, 1);
    threatRow->addWidget(m_threatLabel);

    auto *form = new QFormLayout;
    form->addRow(tr("Game folder"), folderRow);
    form->addRow(tr("Music"), playRow);
    form->addRow(tr("Battle threat"), threatRow);
    form->addRow(tr("Music level"), m_levelLabel);
    form->addRow(QString(), resetButton);

    // Layers
    auto *layersBox = new QGroupBox(tr("Layers"), this);
    auto *layersForm = new QFormLayout(layersBox);
    const std::array<QString, dk2::Engine::LayerCount> names{tr("Music"), tr("Ambience"), tr("Atmosphere")};
    for (int i = 0; i < dk2::Engine::LayerCount; ++i) {
        auto *slider = volumeSlider(layersBox);
        m_nowPlaying[i] = new QLabel(tr("–"), layersBox);
        m_nowPlaying[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *row = new QHBoxLayout;
        row->addWidget(slider);
        row->addWidget(m_nowPlaying[i], 2);
        layersForm->addRow(names[i], row);
        const auto layer = dk2::Engine::LayerId(i);
        connect(slider, &QSlider::valueChanged, this, [this, layer](int value) {
            m_engine.setLayerGain(layer, value / 100.0f);
        });
    }
    auto *master = volumeSlider(layersBox);
    layersForm->addRow(tr("Master"), master);
    connect(master, &QSlider::valueChanged, this, [this](int value) { m_engine.setMasterGain(value / 100.0f); });

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(layersBox);
    layout->addWidget(m_log, 1);
    resize(720, 560);

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browse);
    connect(m_gameDir, &QLineEdit::editingFinished, this, &MainWindow::openGameDir);
    connect(m_play, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    connect(m_track, &QComboBox::currentIndexChanged, this, [this] {
        if (m_engine.isPlaying())
            m_engine.start(m_track->currentData().toInt());
    });
    connect(m_threat, &QSlider::valueChanged, this, [this](int value) {
        m_engine.setThreat(value);
        updateThreatLabel();
    });
    connect(resetButton, &QPushButton::clicked, this, [this] { m_engine.resetLevel(); });
    connect(&m_engine, &dk2::Engine::levelChanged, this, &MainWindow::onLevelChanged);
    connect(&m_engine, &dk2::Engine::itemStarted, this, &MainWindow::onItemStarted);
    connect(&m_engine, &dk2::Engine::errorOccurred, this, [this](const QString &message) { log(tr("Error: %1").arg(message)); });

    onLevelChanged(m_engine.level(), m_engine.intensity());
    if (!m_gameDir->text().isEmpty())
        openGameDir();
    updateThreatLabel();
}

void MainWindow::browse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Dungeon Keeper II folder"), m_gameDir->text());
    if (!dir.isEmpty()) {
        m_gameDir->setText(dir);
        openGameDir();
    }
}

void MainWindow::openGameDir()
{
    const bool playing = m_engine.isPlaying();
    if (m_engine.open(m_gameDir->text())) {
        QSettings().setValue(GameDirKey, m_gameDir->text());
        log(tr("Opened %1").arg(m_gameDir->text()));
        if (playing)
            m_engine.start(m_track->currentData().toInt());
    }
    m_play->setText(m_engine.isPlaying() ? tr("Stop") : tr("Play"));
    m_play->setEnabled(m_engine.isOpen());
    updateThreatLabel();
}

void MainWindow::togglePlayback()
{
    if (m_engine.isPlaying())
        m_engine.stop();
    else if (m_engine.start(m_track->currentData().toInt()))
        log(tr("Level start: silent until the first fight"));
    m_play->setText(m_engine.isPlaying() ? tr("Stop") : tr("Play"));
}

void MainWindow::updateThreatLabel()
{
    const int threat = m_threat->value();
    const QList<int> thresholds = m_engine.thresholds();
    QStringList marks;
    for (int t : thresholds)
        marks.append(QString::number(t));
    m_threatLabel->setText(threat == 0 ? tr("no fight") : tr("%1").arg(threat));
    m_threat->setToolTip(tr("Summed threat of the enemies in the battle. Levels 2-4 start at %1.")
                             .arg(marks.join(QStringLiteral(", "))));
}

void MainWindow::onLevelChanged(int level, int intensity)
{
    m_levelLabel->setText(level == dk2::MusicLevel::Silent ? tr("silent (intensity %1)").arg(intensity)
                                                           : tr("%1 (intensity %2)").arg(level).arg(intensity));
    log(tr("Music level %1, intensity %2").arg(level).arg(intensity));
}

void MainWindow::onItemStarted(dk2::Engine::LayerId layer, int item, const QString &description)
{
    Q_UNUSED(item);
    m_nowPlaying[layer]->setText(description);
    if (layer == dk2::Engine::Music)
        log(description);
}

void MainWindow::log(const QString &line)
{
    const qint64 ms = m_clock.elapsed();
    m_log->appendPlainText(QStringLiteral("%1:%2  %3")
                               .arg(ms / 60000, 2, 10, QLatin1Char('0'))
                               .arg((ms / 1000) % 60, 2, 10, QLatin1Char('0'))
                               .arg(line));
}
