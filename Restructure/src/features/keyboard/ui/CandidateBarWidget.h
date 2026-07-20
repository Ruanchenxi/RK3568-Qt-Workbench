/**
 * @file CandidateBarWidget.h
 * @brief 拼音候选栏组件
 */

#ifndef CANDIDATEBARWIDGET_H
#define CANDIDATEBARWIDGET_H

#include <QWidget>

class QPushButton;

class CandidateBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CandidateBarWidget(QWidget *parent = nullptr);

    void setCandidates(const QStringList &candidates);
    void clearCandidates();
    bool hasCandidates() const;

signals:
    void candidateChosen(int index);

private slots:
    void onNextPage();
    void onPreviousPage();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void resetPaging();

    QStringList m_candidates;
    QList<QRect> m_candidateRects;
    QList<int> m_pageHeadIndexes;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    int m_pageIndex;
    int m_headIndex;
    int m_tailIndex;
};

#endif // CANDIDATEBARWIDGET_H
