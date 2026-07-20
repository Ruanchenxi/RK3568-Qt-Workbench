/**
 * @file KeyboardPinyinEngine.cpp
 * @brief 键盘拼音输入引擎实现
 */

#include "features/keyboard/application/KeyboardPinyinEngine.h"

#include "features/keyboard/application/pinyindecoderservice_p.h"

#include <QPointer>
#include <QVector>

class KeyboardPinyinEnginePrivate
{
    Q_DECLARE_PUBLIC(KeyboardPinyinEngine)

public:
    enum State
    {
        Idle,
        Input
    };

    explicit KeyboardPinyinEnginePrivate(KeyboardPinyinEngine *q)
        : q_ptr(q),
          chineseMode(true),
          decoder(PinyinDecoderService::getInstance()),
          state(Idle),
          surface(),
          totalChoicesNum(0),
          candidatesList(),
          fixedLen(0),
          composingStr(),
          activeCmpsLen(0),
          finishSelection(true),
          posDelSpl(-1),
          isPosInSpl(false)
    {
        if (decoder)
        {
            decoder->setUserDictionary(true);
        }
    }

    void emitCandidates()
    {
        Q_Q(KeyboardPinyinEngine);
        emit q->candidatesChanged(candidatesList);
    }

    void emitPreedit(const QString &text)
    {
        Q_Q(KeyboardPinyinEngine);
        emit q->preeditChanged(text);
    }

    void emitCommit(const QString &text)
    {
        Q_Q(KeyboardPinyinEngine);
        if (!text.isEmpty())
        {
            emit q->committed(text);
        }
    }

    void resetCandidates()
    {
        candidatesList.clear();
        totalChoicesNum = 0;
        emitCandidates();
    }

    void resetToIdle()
    {
        state = Idle;
        surface.clear();
        fixedLen = 0;
        finishSelection = true;
        composingStr.clear();
        activeCmpsLen = 0;
        posDelSpl = -1;
        isPosInSpl = false;
        emitPreedit(QString());
        resetCandidates();
        if (decoder)
        {
            decoder->resetSearch();
        }
    }

    bool addSpellingChar(QChar ch, bool reset)
    {
        if (!decoder)
        {
            return false;
        }
        if (reset)
        {
            surface.clear();
            decoder->resetSearch();
        }
        if (ch == Qt::Key_Apostrophe)
        {
            if (surface.isEmpty())
                return false;
            if (surface.endsWith(ch))
                return true;
        }
        surface.append(ch);
        return true;
    }

    bool removeSpellingChar()
    {
        if (!decoder || surface.isEmpty())
            return false;
        QVector<int> splStart = decoder->spellingStartPositions();
        if (splStart.size() <= fixedLen + 1)
            return false;
        isPosInSpl = (surface.length() <= splStart[fixedLen + 1]);
        posDelSpl = isPosInSpl ? fixedLen - 1 : surface.length() - 1;
        return true;
    }

    QString candidateAt(int index)
    {
        if (!decoder || index < 0 || index >= totalChoicesNum)
            return QString();
        if (index >= candidatesList.size())
        {
            const int fetchMore = qMin(index + 20, totalChoicesNum - candidatesList.size());
            candidatesList.append(decoder->fetchCandidates(candidatesList.size(), fetchMore, fixedLen));
        }
        return index < candidatesList.size() ? candidatesList[index] : QString();
    }

    void updateFromDecoder()
    {
        if (!decoder)
        {
            return;
        }

        resetCandidates();
        totalChoicesNum = decoder->search(surface);
        surface = decoder->pinyinString(false);
        QVector<int> splStart = decoder->spellingStartPositions();
        const QString fullSent = decoder->candidateAt(0);
        fixedLen = decoder->fixedLength();

        if (splStart.size() > fixedLen + 1)
        {
            composingStr = fullSent.mid(0, fixedLen) + surface.mid(splStart[fixedLen + 1]);
        }
        else
        {
            composingStr = surface;
        }

        int surfaceDecodedLen = decoder->pinyinStringLength(true);
        QString display;
        if (!surfaceDecodedLen)
        {
            display = composingStr.toLower();
            if (!totalChoicesNum)
                totalChoicesNum = 1;
        }
        else
        {
            display = fullSent.mid(0, fixedLen);
            for (int pos = fixedLen + 1; pos < splStart.size() - 1; ++pos)
            {
                display += surface.mid(splStart[pos], splStart[pos + 1] - splStart[pos]).toUpper();
                if (splStart[pos + 1] < surfaceDecodedLen)
                    display += QLatin1String("'");
            }
            if (surfaceDecodedLen < surface.length())
                display += surface.mid(surfaceDecodedLen).toLower();
        }

        emitPreedit(display);
        for (int i = 0; i < totalChoicesNum; ++i)
        {
            candidatesList.append(candidateAt(i));
        }
        emitCandidates();
        state = Input;
    }

    void choose(int index)
    {
        if (!decoder || index < 0 || index >= totalChoicesNum)
        {
            return;
        }

        const QString result = candidateAt(index);
        resetToIdle();
        emitCommit(result);
    }

    KeyboardPinyinEngine *q_ptr;
    bool chineseMode;
    QPointer<PinyinDecoderService> decoder;
    State state;
    QString surface;
    int totalChoicesNum;
    QStringList candidatesList;
    int fixedLen;
    QString composingStr;
    int activeCmpsLen;
    bool finishSelection;
    int posDelSpl;
    bool isPosInSpl;
};

KeyboardPinyinEngine::KeyboardPinyinEngine(QObject *parent)
    : QObject(parent),
      d_ptr(new KeyboardPinyinEnginePrivate(this))
{
}

KeyboardPinyinEngine::~KeyboardPinyinEngine()
{
    delete d_ptr;
}

bool KeyboardPinyinEngine::isChineseMode() const
{
    Q_D(const KeyboardPinyinEngine);
    return d->chineseMode;
}

bool KeyboardPinyinEngine::hasCandidates() const
{
    Q_D(const KeyboardPinyinEngine);
    return !d->candidatesList.isEmpty();
}

QString KeyboardPinyinEngine::languageName() const
{
    return isChineseMode() ? QStringLiteral("简体中文") : QStringLiteral("English");
}

QString KeyboardPinyinEngine::preeditText() const
{
    Q_D(const KeyboardPinyinEngine);
    return d->composingStr;
}

QStringList KeyboardPinyinEngine::candidates() const
{
    Q_D(const KeyboardPinyinEngine);
    return d->candidatesList;
}

void KeyboardPinyinEngine::toggleLanguage()
{
    Q_D(KeyboardPinyinEngine);
    d->chineseMode = !d->chineseMode;
    d->resetToIdle();
    emit languageChanged(languageName());
}

void KeyboardPinyinEngine::reset()
{
    Q_D(KeyboardPinyinEngine);
    d->resetToIdle();
}

bool KeyboardPinyinEngine::handleKey(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);
    Q_D(KeyboardPinyinEngine);

    if (!d->chineseMode)
    {
        return false;
    }

    if ((text.size() == 1 && text.at(0).isLetter()) || key == Qt::Key_Apostrophe)
    {
        if (d->addSpellingChar(text.isEmpty() ? QChar('\'') : text.at(0), d->state == KeyboardPinyinEnginePrivate::Idle))
        {
            d->updateFromDecoder();
            return true;
        }
    }
    else if (key == Qt::Key_Backspace)
    {
        if (d->state == KeyboardPinyinEnginePrivate::Idle)
        {
            return false;
        }
        if (d->surface.isEmpty())
        {
            d->resetToIdle();
            return true;
        }
        d->surface.chop(1);
        if (d->surface.isEmpty())
        {
            d->resetToIdle();
        }
        else
        {
            d->updateFromDecoder();
        }
        return true;
    }
    else if ((key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter) && !d->candidatesList.isEmpty())
    {
        d->choose(0);
        return true;
    }

    return false;
}

void KeyboardPinyinEngine::chooseCandidate(int index)
{
    Q_D(KeyboardPinyinEngine);
    d->choose(index);
}
