#include "mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QScreen>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QTimer>

#include <chrono>
#include <atomic>

void CounterManager::addCounter(int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_.push_back(value);
}

void CounterManager::deleteCounter(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= 0 && index < static_cast<int>(counters_.size())) {
        counters_.erase(counters_.begin() + index);
    }
}

std::vector<int> CounterManager::getCounters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return counters_;
}

void CounterManager::incrementAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& counter : counters_) {
        ++counter;
    }
}

void CounterManager::setCounters(const std::vector<int>& counters) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_ = counters;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    loadCountersFromDatabase();
    adjustWindowSize();

    workerThread = std::thread([this]() {
        while (keepRunning.load()) {
            counterManager.incrementAll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    tableTimer = new QTimer(this);
    connect(tableTimer, &QTimer::timeout, this, &MainWindow::updateTable);
    tableTimer->start(100);

    freqTimer = new QTimer(this);
    connect(freqTimer, &QTimer::timeout, this, &MainWindow::updateFrequency);
    freqTimer->start(1000);
}

MainWindow::~MainWindow() {
    keepRunning.store(false);

    if (workerThread.joinable()) {
        workerThread.join();
    }
    QSqlDatabase::database().close();
}

void MainWindow::setupUI() {
    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(1);
    tableWidget->setHorizontalHeaderLabels({"Value"});
    tableWidget->horizontalHeader()->setStretchLastSection(true);

    addButton = new QPushButton("Add", this);
    deleteButton = new QPushButton("Delete", this);
    saveButton = new QPushButton("Save", this);
    freqLabel = new QLabel("Frequency: 0 Hz", this);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(tableWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(saveButton);

    layout->addLayout(buttonLayout);
    layout->addWidget(freqLabel);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddClicked);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
}

void MainWindow::adjustWindowSize() {
    int rowHeight = tableWidget->rowHeight(0);
    int headerHeight = tableWidget->horizontalHeader()->height();
    int totalTableHeight = rowHeight * tableWidget->rowCount() + headerHeight;

    int extraHeight = 150;
    int totalHeight = totalTableHeight + extraHeight;
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();


    int maxHeight = screenGeometry.height() - 100;
    int finalHeight = std::min(totalHeight, maxHeight);

    // Resize and recenter
    resize(tableWidget->width(), finalHeight);
    move(screenGeometry.center() - rect().center());
}

void MainWindow::loadCountersFromDatabase() {
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.isValid()) {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("counters.db");
    }

    if (!db.open()) {
        QMessageBox::critical(this, "Error", "Failed to open database");
        return;
    }

    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS counters (value INTEGER)");
    query.exec("SELECT value FROM counters");

    std::vector<int> counters;
    while (query.next()) {
        counters.push_back(query.value(0).toInt());
    }

    counterManager.setCounters(counters);
    tableWidget->setRowCount(static_cast<int>(counters.size()));
    for (int i = 0; i < static_cast<int>(counters.size()); ++i) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(counters[i]));
        tableWidget->setItem(i, 0, item);
    }

    db.close();
}

void MainWindow::onAddClicked() {
    counterManager.addCounter(0);
    int row = tableWidget->rowCount();
    tableWidget->insertRow(row);
    QTableWidgetItem *item = new QTableWidgetItem("0");
    tableWidget->setItem(row, 0, item);
    adjustWindowSize();
}

void MainWindow::onDeleteClicked() {
    QList<QTableWidgetItem*> selected = tableWidget->selectedItems();
    if (selected.isEmpty()) return;

    int row = selected.first()->row();
    counterManager.deleteCounter(row);
    tableWidget->removeRow(row);

    // Select the next row
    int rowCount = tableWidget->rowCount();
    if (rowCount > 0) {
        int nextRow = row;
        if (nextRow >= rowCount) {
            nextRow = rowCount - 1;
        }
        tableWidget->selectRow(nextRow);
    }
    adjustWindowSize();
}

void MainWindow::onSaveClicked() {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, "Error", "Database connection is not open");
        return;
    }

    QSqlQuery query(db);
    query.exec("BEGIN TRANSACTION");
    query.exec("DELETE FROM counters");

    std::vector<int> counters = counterManager.getCounters();
    query.prepare("INSERT INTO counters (value) VALUES (?)");
    for (int value : counters) {
        query.bindValue(0, value);
        query.exec();
    }

    query.exec("COMMIT");
}

void MainWindow::updateTable() {
    std::vector<int> counters = counterManager.getCounters();
    tableWidget->setRowCount(static_cast<int>(counters.size()));

    for (int i = 0; i < static_cast<int>(counters.size()); ++i) {
        auto *item = tableWidget->item(i, 0);
        if (!item) {
            item = new QTableWidgetItem();
            tableWidget->setItem(i, 0, item);
        }
        item->setText(QString::number(counters[i]));
    }
}

void MainWindow::updateFrequency() {
        std::vector<int> counters = counterManager.getCounters();
        double currentSum = std::accumulate(counters.begin(), counters.end(), 0);

        if (!elapsedTimer.isValid()) {
            elapsedTimer.start();
            previousSum = currentSum;
            return;
        }

        double timeDiff = elapsedTimer.elapsed() / 1000.0;
        if (timeDiff <= 0) return;

        double frequency = (currentSum - previousSum) / timeDiff;
        freqLabel->setText(QString("Frequency: %1 Hz").arg(frequency, 0, 'f', 2));

        previousSum = currentSum;
        elapsedTimer.restart();
}
