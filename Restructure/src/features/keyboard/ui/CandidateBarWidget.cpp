/**
 * @file CandidateBarWidget.cpp
 * @brief 拼音候选栏组件实现
 */

#include "features/keyboard/ui/CandidateBarWidget.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpacerItem>

CandidateBarWidget::CandidateBarWidget(QWidget *parent)
    : QWidget(parent),
      m_prevButton(new QPushButton(QStringLiteral("<"), this)),
      m_nextButton(new QPushButton(QStringLiteral(">"), this)),
      m_pageIndex(0),
      m_headIndex(0),
      m_tailIndex(0)
{
    m_prevButton->setMinimumSize(36, 28);
    m_nextButton->setMinimumSize(36, 28);
    m_prevButton->setFocusPolicy(Qt::NoFocus);
    m_nextButton->setFocusPolicy(Qt::NoFocus);

    connect(m_prevButton, &QPushButton::clicked, this, &CandidateBarWidget::onPreviousPage);
    connect(m_nextButton, &QPushButton::clicked, this, &CandidateBarWidget::onNextPage);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 4, 10, 4);
    layout->setSpacing(6);
    layout->addWidget(m_prevButton);
    layout->addWidget(m_nextButton);
    layout->addItem(new QSpacerItem(80, 24, QSizePolicy::Expanding, QSizePolicy::Minimum));

    setMinimumHeight(38);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFont(QFont(QStringLiteral("Microsoft YaHei"), 11));
    clearCandidates();
}

void CandidateBarWidget::setCandidates(const QStringList &candidates)
{
    m_candidates = candidates;
    m_prevButton->setVisible(!m_candidates.isEmpty());
    m_nextButton->setVisible(!m_candidates.isEmpty());
    resetPaging();
    update();
}

void CandidateBarWidget::clearCandidates()
{
    m_candidates.clear();
    m_prevButton->setVisible(false);
    m_nextButton->setVisible(false);
    resetPaging();
    update();
}

bool CandidateBarWidget::hasCandidates() const
{
    return !m_candidates.isEmpty();
}

void CandidateBarWidget::onNextPage()
{
    if (m_tailIndex >= m_candidates.size() - 1)
    {
        return;
    }

    m_headIndex = m_tailIndex + 1;
    ++m_pageIndex;
    if (m_pageIndex >= m_pageHeadIndexes.size())
    {
        m_pageHeadIndexes.append(m_headIndex);
    }
    update();
}

void CandidateBarWidget::onPreviousPage()
{
    if (m_pageIndex == 0)
    {
        return;
    }

    --m_pageIndex;
    m_headIndex = m_pageHeadIndexes.at(m_pageIndex);
    update();
}

void CandidateBarWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setPen(QColor(QStringLiteral("#F7F9FA")));

    if (m_candidates.isEmpty())
    {
        m_prevButton->setEnabled(false);
        m_nextButton->setEnabled(false);
        return;
    }

    QRect drawRect = m_nextButton->geometry();
    drawRect.setLeft(drawRect.right() + 14);
    drawRect.setRight(width() - 10);

    m_candidateRects.clear();
    m_tailIndex = m_headIndex - 1;
    for (int i = m_headIndex; i < m_candidates.size(); ++i)
    {
        const QString text = m_candidates.at(i);
        QRect textRect = painter.boundingRect(drawRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        if (textRect.right() + 24 >= width())
        {
            break;
        }

        painter.drawText(drawRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        m_candidateRects.append(textRect);
        m_tailIndex = i;
        drawRect.translate(textRect.width() + 24, 0);
    }

    m_prevButton->setEnabled(m_pageIndex > 0);
    m_nextButton->setEnabled(m_tailIndex < m_candidates.size() - 1);
}

void CandidateBarWidget::mousePressEvent(QMouseEvent *event)
{
    if (!event)
    {
        return;
    }

    for (int i = 0; i < m_candidateRects.size(); ++i)
    {
        if (m_candidateRects.at(i).contains(event->pos()))
        {
            emit candidateChosen(m_headIndex + i);
            break;
        }
    }
}

void CandidateBarWidget::resetPaging()
{
    m_pageIndex = 0;
    m_headIndex = 0;
    m_tailIndex = -1;
    m_candidateRects.clear();
    m_pageHeadIndexes.clear();
    m_pageHeadIndexes.append(0);
    m_prevButton->setEnabled(false);
    m_nextButton->setEnabled(false);
}
