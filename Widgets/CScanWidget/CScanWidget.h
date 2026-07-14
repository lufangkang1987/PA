#pragma once
#include <QWidget>
#include <QImage>
#include <QVector>

class CScanWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CScanWidget(QWidget *parent = nullptr);

    /// 设置 C扫 振幅数据（采集时由 Driver/HomePage 调用）
    /// @param data  振幅数组，行优先 (row-major)，值域 0.0~1.0
    /// @param w     图像宽度（X 方向采样点数）
    /// @param h     图像高度（Y 方向采样点数）
    void setData(const QVector<float> &data, int w, int h);

    /// 获取当前 C扫 数据（供保存用）
    QVector<float> data() const { return m_data; }
    int dataWidth()  const { return m_dataWidth; }
    int dataHeight() const { return m_dataHeight; }
    bool hasData()   const { return !m_data.isEmpty(); }

    /// 回放模式（显示 "REPLAY" 标记）
    void setReplayMode(bool on) { m_replay = on; update(); }
    bool isReplay() const { return m_replay; }

    /// 清空数据，回到 mock 显示
    void clearData();
    void setSelectedLine(int line);
    void setAnalysisRect(int line1, int line2, int column1, int column2);
    void setPhysicalScale(float scanStepMm, float imageStartMm, float imageEndMm);
    void setPageStart(int line);
    void setImageColumnRange(int first, int last);
    int pageStart() const { return m_pageStart; }

signals:
    void positionSelected(int line, int imageColumn);
    void analysisRectChanged(int line1, int line2, int column1, int column2);
    void analysisMeasured(float maximum, float average, int maxLine, int maxColumn);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void rebuildMock(const QSize &size);
    void rebuildFromData();

    QImage m_cache;

    // 真实数据
    QVector<float> m_data;
    int m_dataWidth  = 0;
    int m_dataHeight = 0;
    bool m_replay    = false;
    int m_selectedLine = -1;
    int m_analysisLine1 = 110;
    int m_analysisLine2 = 140;
    int m_analysisColumn1 = 100;
    int m_analysisColumn2 = 120;
    float m_scanStepMm = 0.5f;
    float m_imageStartMm = 0.0f;
    float m_imageEndMm = 512.0f;
    int m_pageStart = 0;
    int m_imageColumnStart = 0;
    int m_imageColumnEnd = 512;
    static constexpr int PageLineCount = 925;
};
