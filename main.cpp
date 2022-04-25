#define _USE_MATH_DEFINES
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <list>
#include <set>
#include <string>
#include <vector>

using namespace std;
using Actions = vector<string>;

struct BaseStats {
    int health, mana;
};
enum EntityType {
    Monster,
    Our,
    Enemy
};
struct Bases {
    BaseStats ourBase, enemyBase;
    void read() {
        cin >> ourBase.health >> ourBase.mana; cin.ignore();
        cin >> enemyBase.health >> enemyBase.mana; cin.ignore();
    }
} bases;
struct Coord {
    int x, y;
};
double dist(Coord c1, Coord c2) {
    int dx = abs(c1.x - c2.x), dy = abs(c1.y - c2.y);
    return sqrt(dx * dx + dy * dy);
}
const int cBaseRadius{ 5000 };
const Coord maxCoord{ 17630, 9000 };
const double cMaxDist = dist({ 0, 0 }, maxCoord);

struct Entity {
    int id, type, shieldLife, isControlled, health, vx, vy, nearBase, threatFor;
    Coord coords;

    static const int cMonsterSpeed = 400;
    static const int cUnitDamage = 2;

    // custom fileds
    double importance{ 0. };
    double distanceToBase{ 0. };
    int stepsToBase{ 0 };
    int neededUnitsNb{ 0 }; // МИНИМАЛЬНОЕ количество юнитов, требующееся для убийства монстра

    void read() {
        cin >> id >> type >> coords.x >> coords.y >> shieldLife >> isControlled >> health >> vx >> vy >> nearBase >> threatFor; cin.ignore();
    }
};
bool entityCompare(const Entity& left, const Entity& right)
{
    return (left.importance > right.importance);
}

struct Entities {
    vector<Entity> our, enemies, monsters;
    Coord baseCoords;
    int entitiesNb;
    Entity tempEntity;

    vector<Entity> dangerousMonsters;

    vector<vector<Coord>> waitCoordsVec2;

    void readBaseCoords() {
        cin >> baseCoords.x >> baseCoords.y; cin.ignore();
        waitCoordsVec2 = getWaitCoords();
    }

    void read() {
        monsters.clear(); our.clear(); enemies.clear(); dangerousMonsters.clear();
        cin >> entitiesNb; cin.ignore();
        for (int entityId = 0; entityId < entitiesNb; entityId++) {
            tempEntity.read();
            switch (tempEntity.type) {
            case Monster: monsters.push_back(tempEntity); break;
            case Our:     our.push_back(tempEntity); break;
            case Enemy:   enemies.push_back(tempEntity); break;
            default: break;
            }
        }
    }

    Actions updateDangerousMonsters() {
        int maxSimulationDepth = 70;
        set<int> accountedInDistToBaseIdxs; // учтенные в расчете дистанции до базы (попадают в этот сет, когда расстояние до базы <= 400)
        for (int simulationDepth = 0; simulationDepth < maxSimulationDepth; simulationDepth++) {
            // simulate monster pos for simulationDepth iteration
            for (Entity& monster : monsters) {

                Coord simulatedCoord = { monster.coords.x + monster.vx, monster.coords.y + monster.vy };

                // if (dist(monster.coords, baseCoords) <= cBaseRadius) {
                //     simulatedCoord = { monster.coords.x + monster.vx, monster.coords.y + monster.vy };
                // }
                // else {
                //     simulatedCoord = { monster.coords.x + simulationDepth * monster.vx, monster.coords.y + simulationDepth * monster.vy };
                // }

                double distToBase = dist(simulatedCoord, baseCoords);

                // добавляем дистанцию к общей дистанции до базы за тот ход, который симулируем
                // если монстр у базы, то для него в последующих симуляциях не надо считать дистанцию
                if (!accountedInDistToBaseIdxs.count(monster.id)) {

                    monster.distanceToBase += Entity::cMonsterSpeed;
                    monster.stepsToBase++;

                    if (distToBase < 700) {
                        monster.importance = (maxSimulationDepth - simulationDepth - 1) * 2500.;
                        monster.neededUnitsNb = static_cast<int>(std::ceil(static_cast<double>(monster.health) / (monster.stepsToBase * Entity::cUnitDamage)));

                        accountedInDistToBaseIdxs.insert(monster.id);
                        dangerousMonsters.push_back(monster);
                    }

                }
            }
        }

        cerr << "Sorting dangerous monsters, size: " << dangerousMonsters.size() << endl;

        // сортировка по важности
        sort(dangerousMonsters.begin(), dangerousMonsters.end(), entityCompare);

        cerr << "Creating actions" << endl;

        // формируем ходы для юнитов на основании важности монстра и количества юнитов, которое необходимо для его убийства
        Actions actions;
        actions.resize(3);
        for (Entity& monster : dangerousMonsters) {

            cerr << "Monster with id" << monster.id << " has importance " << monster.importance
                << ", dist to base " << monster.distanceToBase << ", stepsToBase " << monster.stepsToBase
                << " and need units: " << monster.neededUnitsNb << endl;

            for (int neededUnitIdx = 0; neededUnitIdx < monster.neededUnitsNb; neededUnitIdx++) {
                // ищем ближайшего юнита к данному монстру
                double bestDist = cMaxDist, curDist;
                int bestOurUnitId = -1;
                for (Entity& ourUnit : our) {
                    curDist = dist(ourUnit.coords, monster.coords);
                    if (curDist < bestDist) {
                        bestDist = curDist;
                        bestOurUnitId = ourUnit.id;
                    }
                }

                cerr << "\tBest our unit id" << bestOurUnitId << endl;

                // определяем индекс команды
                int commandIdx = bestOurUnitId - (bestOurUnitId >= 3 ? 3 : 0);

                // добавляем команду на атаку ближайшим юнитом этого монстра
                // ТУТ МОЖНО ДОБАВИТЬ ВЕТЕРОК, ЕСЛИ МОНСТР НЕ МЕЖДУ БАЗОЙ И ЮНИТОМ
                actions[commandIdx] = "MOVE " + std::to_string(monster.coords.x) + " " + std::to_string(monster.coords.y);

                // удаляем выбранного юнита из доступных для хода
                for (int i = 0; i < our.size(); i++) {
                    if (our[i].id == bestOurUnitId) {
                        our.erase(our.begin() + i);
                        break;
                    }
                }

                if (our.empty()) break;
            }

            if (our.empty()) break;
        }

        if (our.size()) {
            cerr << "Creating wait actions for " << our.size() << " our units" << endl;
            for (int i = 0; i < our.size(); i++) {
                // определяем индекс команды
                int commandIdx = our[i].id - (our[i].id >= 3 ? 3 : 0);
                cerr << "\tCommand for unit " << commandIdx << "(" << our[i].id << ")" << endl;
                actions[commandIdx] = "MOVE " + std::to_string(waitCoordsVec2[our.size() - 1][i].x) + " " + std::to_string(waitCoordsVec2[our.size() - 1][i].y);
                cerr << "\tAction: " << actions[commandIdx] << endl;
            }
        }

        return actions;
    }

    vector<vector<Coord>> getWaitCoords() {
        static const double defendRadiusCoef = 1.1;

        cerr << "Base coords: " << baseCoords.x << " " << baseCoords.y << endl;

        return {
            vector<Coord> {
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef))) }
            },
            vector<Coord> {
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(22.5 * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(22.5 * M_PI / 180.)))) },
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(67.5 * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(67.5 * M_PI / 180.)))) }
            },
            vector<Coord> {
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(22.5 * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(22.5 * M_PI / 180.)))) },
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(45.0 * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(45.0 * M_PI / 180.)))) },
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(67.5 * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(67.5 * M_PI / 180.)))) }
            }
        };
    }
} entities;

int main() {
    entities.readBaseCoords();
    int heroesNb; cin >> heroesNb; cin.ignore();

    while (1) {
        bases.read();
        entities.read();

        auto startTime = std::chrono::high_resolution_clock::now();
        for (const string& action : entities.updateDangerousMonsters()) {
            cout << action << endl;
        }
        cerr << "time per turn: " << std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count() << " ms\n";
    }
}