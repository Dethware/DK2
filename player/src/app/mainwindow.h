#pragma once

#include "engine.h"

#include <QElapsedTimer>
#include <QWidget>

#include <array>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;

// The UI only: binds widgets to dk2::Engine
class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(const QString &gameDir, QWidget *parent = nullptr);

private:
    void browse();
    void openGameDir();
    void togglePlayback();
    void updateThreatLabel();
    void onLevelChanged(int level, int intensity);
    void onItemStarted(dk2::Engine::LayerId layer, int item, const QString &description);
    void log(const QString &line);

    dk2::Engine m_engine;
    QElapsedTimer m_clock;

    QLineEdit *m_gameDir = nullptr;
    QComboBox *m_track = nullptr;
    QPushButton *m_play = nullptr;
    QSlider *m_threat = nullptr;
    QLabel *m_threatLabel = nullptr;
    QLabel *m_levelLabel = nullptr;
    std::array<QLabel *, dk2::Engine::LayerCount> m_nowPlaying{};
    QPlainTextEdit *m_log = nullptr;
};
