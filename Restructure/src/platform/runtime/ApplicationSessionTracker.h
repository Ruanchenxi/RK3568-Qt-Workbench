#ifndef APPLICATIONSESSIONTRACKER_H
#define APPLICATIONSESSIONTRACKER_H

#include <QObject>
#include <QString>

class ApplicationSessionTracker : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationSessionTracker(QObject *parent = nullptr);

    // Creates this run's diagnostic marker and reports an earlier unclean exit.
    void start();

public slots:
    // Removes only the runtime marker; it never persists business state.
    void markCleanExit();

private:
    QString markerPath() const;
    bool m_started;
};

#endif
