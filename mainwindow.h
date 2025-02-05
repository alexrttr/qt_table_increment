#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>

#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

class CounterManager {
     Q_DISABLE_COPY(CounterManager)
public:
    CounterManager() = default;
    ~CounterManager() {};
    void addCounter(int value);
    void deleteCounter(int index);
    std::vector<int> getCounters() const;
    void incrementAll();
    void setCounters(const std::vector<int>& counters);

private:
    mutable std::mutex mutex_;
    std::vector<int> counters_;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAddClicked();
    void onDeleteClicked();
    void onSaveClicked();
    void updateTable();
    void updateFrequency();
    void loadCountersFromDatabase();

private:
    void setupUI();
    void adjustWindowSize();

    QTableWidget *tableWidget;
    QPushButton *addButton;
    QPushButton *deleteButton;
    QPushButton *saveButton;
    QLabel *freqLabel;
    QTimer *tableTimer;
    QTimer *freqTimer;

    CounterManager counterManager;
    std::thread workerThread;
    std::atomic<bool> keepRunning{true};

    QElapsedTimer elapsedTimer;
    double previousSum = 0;
};

#endif // MAINWINDOW_H
