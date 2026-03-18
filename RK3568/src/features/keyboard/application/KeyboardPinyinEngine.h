/**
 * @file KeyboardPinyinEngine.h
 * @brief 键盘拼音输入引擎
 */

#ifndef KEYBOARDPINYINENGINE_H
#define KEYBOARDPINYINENGINE_H

#include <QObject>
#include <QStringList>

class KeyboardPinyinEnginePrivate;

class KeyboardPinyinEngine : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(KeyboardPinyinEngine)

public:
    explicit KeyboardPinyinEngine(QObject *parent = nullptr);
    ~KeyboardPinyinEngine();

    bool isChineseMode() const;
    bool hasCandidates() const;
    QString languageName() const;
    QString preeditText() const;
    QStringList candidates() const;

    void toggleLanguage();
    void reset();
    bool handleKey(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers);
    void chooseCandidate(int index);

signals:
    void candidatesChanged(const QStringList &candidates);
    void preeditChanged(const QString &text);
    void committed(const QString &text);
    void languageChanged(const QString &name);

private:
    KeyboardPinyinEnginePrivate *d_ptr;
};

#endif // KEYBOARDPINYINENGINE_H
