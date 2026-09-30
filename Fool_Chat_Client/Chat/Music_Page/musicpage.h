#ifndef MUSICPAGE_H
#define MUSICPAGE_H
/******************************************************************************
*
* @file       musicpage.h
* @brief      音乐页 Function
*
* @author     Fool
* @date       2026/03/08
* @history
*****************************************************************************/
#include "../Chat_Comp/page_base.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "ElaSlider.h"
#include "ElaPushButton.h"

class ElaPromotionView;
class ElaTableView;
class ElaText;
class ElaSlider;
class ElaIconButton;
class QStandardItemModel;

class MusicPage : public Page_Base
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit MusicPage(QWidget *parent = nullptr);
    ~MusicPage();

protected:
    // 进页时重载歌单（配合设置页"清理缓存"即时生效）
    void showEvent(QShowEvent* event) override;

private:
    void setupUI();
    void setupPlayerBar(QVBoxLayout* mainLayout);
    void setupSongList(QVBoxLayout* mainLayout);

    void initDatabase();
    void loadSongsFromDatabase();
    void addRowToTable(int id, QString title, QString artist, QString album, QString duration, QString filePath);
    void adjustTableHeight();

    // 播放器
    void initPlayer();              // 初始化播放器
    void playMusic(int row);        // 播放指定行的音乐
    void togglePlayState();         // 切换 播放/暂停
    void playNext();                // 下一首
    void playPrev();                // 上一首
    void updateDuration(qint64 duration); // 更新总时长
    void updatePosition(qint64 position); // 更新进度
    QString formatTime(qint64 ms);

    // 控件指针
    ElaText* m_pCurrentSongLabel;
    ElaText* m_pCurrentArtistLabel;
    ElaIconButton* m_pPlayBtn;
    ElaSlider* m_pProgressSlider;

    // 数据模型
    QStandardItemModel* m_pTableModel;
    ElaTableView* m_pSongTable;

    // 核心对象
    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    int m_currentRow = -1;
    ElaSlider* m_pVolumeSlider;

    int m_value;

private slots:
    void onAddSong();
    void onDeleteSong();
    void onContextMenu(const QPoint& pos);
};

#endif // MUSICPAGE_H
