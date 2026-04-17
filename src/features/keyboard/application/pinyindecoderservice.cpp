/**
 * @file pinyindecoderservice.cpp
 * @brief 拼音解码服务实现
 */

#include "features/keyboard/application/pinyindecoderservice_p.h"

#include "features/keyboard/third_party/pinyin/include/pinyinime.h"
#include "features/keyboard/third_party/pinyin/include/dictdef.h"

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QtCore/QLibraryInfo>
#include <QDebug>

using namespace ime_pinyin;

QScopedPointer<PinyinDecoderService> PinyinDecoderService::_instance;

PinyinDecoderService::PinyinDecoderService(QObject *parent)
    : QObject(parent),
      initDone(false)
{
}

PinyinDecoderService::~PinyinDecoderService()
{
    if (initDone)
    {
        im_close_decoder();
        initDone = false;
    }
}

PinyinDecoderService *PinyinDecoderService::getInstance()
{
    if (!_instance)
        _instance.reset(new PinyinDecoderService());
    if (!_instance->init())
        return nullptr;
    return _instance.data();
}

bool PinyinDecoderService::init()
{
    if (initDone)
        return true;

    QString sysDict(qgetenv("QT_VIRTUALKEYBOARD_PINYIN_DICTIONARY"));
    if (!QFileInfo::exists(sysDict))
    {
        sysDict = QLatin1String(":/keyboard/dict/dict_pinyin.dat");
        if (!QFileInfo::exists(sysDict))
            sysDict = QLibraryInfo::location(QLibraryInfo::DataPath)
                      + QLatin1String("/qtvirtualkeyboard/pinyin/dict_pinyin.dat");
    }

    QString usrDictPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QFileInfo usrDictInfo(usrDictPath + QLatin1String("/rk3568/keyboard/usr_dict.dat"));
    if (!usrDictInfo.exists())
    {
        QDir().mkpath(usrDictInfo.absolutePath());
    }

    initDone = im_open_decoder(sysDict.toUtf8().constData(),
                               usrDictInfo.absoluteFilePath().toUtf8().constData());
    if (!initDone)
    {
        qWarning() << "Could not initialize pinyin engine. sys_dict:"
                   << sysDict << "usr_dict:" << usrDictInfo.absoluteFilePath();
    }

    return initDone;
}

void PinyinDecoderService::setUserDictionary(bool enabled)
{
    if (enabled == im_is_user_dictionary_enabled())
        return;
    if (enabled)
    {
        QString usrDictPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QFileInfo usrDictInfo(usrDictPath + QLatin1String("/rk3568/keyboard/usr_dict.dat"));
        im_init_user_dictionary(usrDictInfo.absoluteFilePath().toUtf8().constData());
    }
    else
    {
        im_init_user_dictionary(nullptr);
    }
}

bool PinyinDecoderService::isUserDictionaryEnabled() const
{
    return im_is_user_dictionary_enabled();
}

void PinyinDecoderService::setLimits(int maxSpsLen, int maxHzsLen)
{
    if (maxSpsLen <= 0)
        maxSpsLen = kMaxSearchSteps - 1;
    if (maxHzsLen <= 0)
        maxHzsLen = kMaxSearchSteps;
    im_set_max_lens(size_t(maxSpsLen), size_t(maxHzsLen));
}

int PinyinDecoderService::search(const QString &spelling)
{
    QByteArray spellingBuf = spelling.toLatin1();
    return int(im_search(spellingBuf.constData(), spellingBuf.length()));
}

int PinyinDecoderService::deleteSearch(int pos, bool isPosInSpellingId, bool clearFixedInThisStep)
{
    if (pos <= 0)
        pos = 0;
    return int(im_delsearch(size_t(pos), isPosInSpellingId, clearFixedInThisStep));
}

void PinyinDecoderService::resetSearch()
{
    im_reset_search();
}

QString PinyinDecoderService::pinyinString(bool decoded)
{
    size_t py_len;
    const char *py = im_get_sps_str(&py_len);
    if (!decoded)
        py_len = strlen(py);
    return QString(QLatin1String(py, int(py_len)));
}

int PinyinDecoderService::pinyinStringLength(bool decoded)
{
    size_t py_len;
    const char *py = im_get_sps_str(&py_len);
    if (!decoded)
        py_len = strlen(py);
    return int(py_len);
}

QVector<int> PinyinDecoderService::spellingStartPositions()
{
    const unsigned short *spl_start;
    int len = int(im_get_spl_start_pos(spl_start));
    QVector<int> arr;
    arr.resize(len + 2);
    arr[0] = len;
    for (int i = 0; i <= len; i++)
        arr[i + 1] = spl_start[i];
    return arr;
}

QString PinyinDecoderService::candidateAt(int index)
{
    QVector<QChar> candidateBuf;
    candidateBuf.resize(kMaxSearchSteps + 1);
    if (!im_get_candidate(size_t(index), (char16 *)candidateBuf.data(), candidateBuf.length() - 1))
        return QString();
    candidateBuf.last() = 0;
    return QString(candidateBuf.data());
}

QList<QString> PinyinDecoderService::fetchCandidates(int index, int count, int sentFixedLen)
{
    QList<QString> list;
    for (int i = index; i < index + count; ++i)
    {
        QString retStr = candidateAt(i);
        if (i == 0)
            retStr.remove(0, sentFixedLen);
        list.append(retStr);
    }
    return list;
}

int PinyinDecoderService::chooceCandidate(int index)
{
    return int(im_choose(index));
}

int PinyinDecoderService::cancelLastChoice()
{
    return int(im_cancel_last_choice());
}

int PinyinDecoderService::fixedLength()
{
    return int(im_get_fixed_len());
}

void PinyinDecoderService::flushCache()
{
    im_flush_cache();
}

QList<QString> PinyinDecoderService::predictionList(const QString &history)
{
    QList<QString> predictList;
    char16 (*predictItems)[kMaxPredictSize + 1] = nullptr;
    int predictNum = int(im_get_predicts(history.utf16(), predictItems));
    predictList.reserve(predictNum);
    for (int i = 0; i < predictNum; i++)
        predictList.append(QString((QChar *)predictItems[i]));
    return predictList;
}
