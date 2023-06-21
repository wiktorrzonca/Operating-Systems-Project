#include <algorithm>
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <ncurses.h>

std::mutex mtx;
std::vector<std::priority_queue<std::pair<int, int>>> runwayQueues(5);
std::vector<std::pair<int, int>> parking(5, {-1, 0});
std::vector<std::pair<int, int>> runway(3, {-1, 0});
std::vector<int> cooldown(5, 0);
std::vector<std::pair<int, int>> stations(2, {-1, 0});
std::mutex passengerMtx;
std::vector<std::mutex> runwayMutexes(3);
std::vector<std::queue<int>> stationQueues(2);
std::vector<std::mutex> stationMutexes(2);
std::vector<std::mutex> queueMutexes(2);

std::random_device rd;
std::mt19937 gen(rd());

int totalPassengers = std::uniform_int_distribution<>(200, 500)(gen);

int getRandomStation() {
    std::uniform_int_distribution<> dist(1, 2);
    return dist(gen);
}
void graphics(){
    for(int i=0;i<parking.size();i++)
    {
        mvprintw(1+i*7,1,"|--------- PARKING %d --------|",i+1);
    }
    for(int i=0;i<runway.size();i++)
    {
        mvprintw(1+i*7,60,"|--------- RUNWAY %d --------|",i+1);
    }
    for(int i=0;i<stations.size();i++)
    {
        mvprintw(1+i*7,120,"|--------- STATION %d --------|",i+1);
    }
}
void updateScreen() {
    clear();
    graphics();
    std::sort(parking.begin(), parking.end(), [](const std::pair<int, int>& left, const std::pair<int, int>& right) {
        return left.first < right.first;
    });

    for (int i = 0; i < parking.size(); i++) {
        if (parking[i].first != -1) {
            mvprintw(2+i*7, 1, "   Bus %d, Passengers: %d", parking[i].first, parking[i].second);
            mvprintw(3+i*7,1, "     ________________");
            mvprintw(4+i*7,1, "    |_________________|");
            mvprintw(5+i*7,1, "    |__|__|__|__|__|__|");
            mvprintw(6+i*7,1, "    |_________________|");
            mvprintw(7+i*7,1, "        @@       @@    ");
        } else {
            mvprintw(2 + i*7, 1, "            Empty");
        }
    }

    for (int i = 0; i < runway.size(); i++) {
        if (runway[i].first != -1) {
            mvprintw(2 + i*7, 60, "   Bus %d, Passengers: %d", runway[i].first, runway[i].second);
            mvprintw(3+i*7,60, "     ________________");
            mvprintw(4+i*7,60, "    |_________________|");
            mvprintw(5+i*7,60, "    |__|__|__|__|__|__|");
            mvprintw(6+i*7,60, "    |_________________|");
            mvprintw(7+i*7,60, "        @@       @@    ");
        } else {
            mvprintw(2 + i*7, 60, "            Empty");
        }
    }

    for (int i = 0; i < stations.size(); i++) {
        if (stations[i].first != -1) {
            mvprintw(2 + i*7, 120, "   Bus %d, Passengers: %d", stations[i].first, stations[i].second);
            mvprintw(3+i*7,120, "     ________________");
            mvprintw(4+i*7,120, "    |_________________|");
            mvprintw(5+i*7,120, "    |__|__|__|__|__|__|");
            mvprintw(6+i*7,120, "    |_________________|");
            mvprintw(7+i*7,120, "        @@       @@    ");
        } else {
            mvprintw(2 + i*7, 120, "            Empty");
        }
    }

    mvprintw(40, 1, "Total Passengers: %d", totalPassengers);
    mvprintw(40, 60, "Runway 1: Passengers (1-15)");
    mvprintw(41, 60, "Runway 2: Passengers (16-30)");
    mvprintw(42, 60, "Runway 3: Passengers (31-50)");
    refresh();
}


void simulateArrivals() {
    while (true) {
        int newPassengers = rand() % 100 + 1;

        {
            std::unique_lock<std::mutex> passengerLock(passengerMtx);
            totalPassengers += newPassengers;
        }
        int waiting_time = rand() % 5 + 1;
        std::this_thread::sleep_for(std::chrono::seconds(waiting_time));
    }
}

void busSimulation(int busNumber) {
    while (true) {
        int passengers = rand()%50+1;

        {
            std::lock_guard<std::mutex> passengerLock(passengerMtx);
            int passengersToRemove = std::min(passengers, totalPassengers);
            totalPassengers -= passengersToRemove;
        }

        std::unique_lock<std::mutex> lock(mtx);
        bool parked = false;
        while (!parked) {
            for (int i = 0; i < parking.size(); i++) {
                if (parking[i].first == -1) {
                    parking[i] = {busNumber, passengers};
                    parked = true;
                    cooldown[busNumber - 1] = 0;
                    break;
                }
            }
            if (!parked) {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::seconds(1));
                lock.lock();
            }
        }

        updateScreen();
        lock.unlock();

        std::uniform_int_distribution<> waitingTimeDist(3, 4);
        int waiting_time = waitingTimeDist(gen);
        std::this_thread::sleep_for(std::chrono::seconds(waiting_time));

        int runwayIndex;
        if (passengers < 15) {
            runwayIndex = 0;
        } else if (passengers >= 15 && passengers <= 30) {
            runwayIndex = 1;
        } else {
            runwayIndex = 2;
        }

        {
            std::lock_guard<std::mutex> runwayLock(runwayMutexes[runwayIndex]);
            runwayQueues[runwayIndex].push(std::make_pair(passengers, busNumber));
        }

        updateScreen();

        bool inRunway = false;
        while (!inRunway) {
            if (runwayQueues[runwayIndex].top().second == busNumber && cooldown[busNumber - 1] <= 0) {
                std::lock_guard<std::mutex> runwayLock(runwayMutexes[runwayIndex]);
                if (runway[runwayIndex].first == -1) {
                    runway[runwayIndex] = {busNumber, passengers};
                    inRunway = true;
                }
            }
            if (!inRunway) {
                lock.lock();
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::seconds(1));
                cooldown[busNumber - 1]--;
            }
        }

        auto it = std::find_if(parking.begin(), parking.end(), [busNumber](const std::pair<int, int>& p) {
            return p.first == busNumber;
        });
        if (it != parking.end()) {
            *it = {-1, 0};
        }
        {
            std::lock_guard<std::mutex> runwayLock(runwayMutexes[runwayIndex]);
            runwayQueues[runwayIndex].pop();
        }

        updateScreen();

        std::this_thread::sleep_for(std::chrono::seconds(2));

        {
            std::lock_guard<std::mutex> runwayLock(runwayMutexes[runwayIndex]);
            runway[runwayIndex] = {-1, 0};
        }

        std::unique_lock<std::mutex> stationLock(mtx);
        bool atStation = false;
        int stationIndex = getRandomStation();
        if (stations[stationIndex - 1].first == -1) {
            stations[stationIndex - 1] = {busNumber, passengers};
            atStation = true;
        }
        stationLock.unlock();

        updateScreen();

        if (atStation) {
            std::uniform_int_distribution<> stationWaitingTimeDist(3, 5);
            int stationWaitingTime = stationWaitingTimeDist(gen);
            std::this_thread::sleep_for(std::chrono::seconds(stationWaitingTime));

            stationLock.lock();
            stations[stationIndex - 1] = {-1, 0};
            stationLock.unlock();

            updateScreen();
        } else {
            std::unique_lock<std::mutex> queueLock(queueMutexes[stationIndex - 1]);
            stationQueues[stationIndex - 1].push(busNumber);
            queueLock.unlock();

            updateScreen();

            while (!atStation) {
                queueLock.lock();
                if (!stationQueues[stationIndex - 1].empty() && stationQueues[stationIndex - 1].front() == busNumber) {
                    stationQueues[stationIndex - 1].pop();
                    stations[stationIndex - 1] = {busNumber, passengers};
                    atStation = true;
                }
                queueLock.unlock();

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            updateScreen();
        }
    }
}


int main() {
    std::random_device rd;
    std::mt19937 gen(rd());

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    std::thread arrivalThread(simulateArrivals);

    std::vector<std::thread> busThreads;
    for (int i = 1; i <= 5; i++) {
        busThreads.emplace_back(busSimulation, i);
    }
    for (auto& thread : busThreads) {
        thread.join();
    }

    arrivalThread.join();

    endwin();

    return 0;
}