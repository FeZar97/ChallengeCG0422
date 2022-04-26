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
struct Action {
    std::string action;
    int val1;
    int val2{ -1 };
    int val3{ -1 };
};
using Actions = vector<Action>;

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
const double cBaseRadius{ 5000 };
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
    int reachCordonSteps{ 0 };
    Coord simulatedCoords;

    void read() {
        cin >> id >> type >> coords.x >> coords.y >> shieldLife >> isControlled >> health >> vx >> vy >> nearBase >> threatFor; cin.ignore();
    }
};
bool entityCompare(const Entity& left, const Entity& right) {
    return (left.importance > right.importance);
}

struct Entities {
    vector<Entity> our, enemies, monsters;
    Coord baseCoords, enemyBaseCoords;
    int entitiesNb;
    Entity tempEntity;

    vector<Entity> dangerousMonsters, undangerousMonsters;

    vector<vector<Coord>> waitCoordsVec2;

    void readBaseCoords() {
        cin >> baseCoords.x >> baseCoords.y; cin.ignore();
        waitCoordsVec2 = getWaitCoords();
        enemyBaseCoords = { maxCoord.x - baseCoords.x, maxCoord.y - baseCoords.y };
    }

    void read() {
        monsters.clear(); our.clear(); enemies.clear(); dangerousMonsters.clear(); undangerousMonsters.clear();
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
        int maxSimulationDepth = 35;

        for (Entity& monster : monsters) {
            monster.simulatedCoords = monster.coords;
        }
        vector<Entity> safetyOur;
        for (const Entity& ourUnit : our) {
            safetyOur.push_back(ourUnit);
        }

        set<int> accountedDangeroudIdxs; // учтенные в расчете дистанции до базы (попадают в этот сет, когда расстояние до базы <= 400)
        for (int simulationDepth = 0; simulationDepth < maxSimulationDepth; simulationDepth++) {
            // simulate monster pos for simulationDepth iteration
            for (Entity& monster : monsters) {

                monster.simulatedCoords.x += monster.vx;
                monster.simulatedCoords.y += monster.vy;

                double distToBase = dist(monster.simulatedCoords, baseCoords);

                // меняеем вектор скорости, когда монстр заходит в радиус базы
                if (distToBase < cBaseRadius)
                {
                    int dBaseX = baseCoords.x - monster.simulatedCoords.x,
                        dBaseY = baseCoords.y - monster.simulatedCoords.y;
                    monster.vx = Entity::cMonsterSpeed * dBaseX / distToBase;
                    monster.vy = Entity::cMonsterSpeed * dBaseY / distToBase;

                    if (!monster.reachCordonSteps)
                    {
                        monster.reachCordonSteps = simulationDepth;
                        // cerr << "Monster " << monster.id << " will reach our base cordon in " << monster.reachCordonSteps << " steps" << endl;
                        // cerr << "Monster " << monster.id << " change his V to " << monster.vx << " " << monster.vy << endl;
                    }

                    // cerr << "On simulation " << simulationDepth << " monster " << monster.id << " speed: " << monster.vx << " " << monster.vy << endl;
                }

                // добавляем дистанцию к общей дистанции до базы за тот ход, который симулируем
                // если монстр у базы, то для него в последующих симуляциях не надо считать дистанцию
                if (!accountedDangeroudIdxs.count(monster.id)) {

                    monster.distanceToBase += Entity::cMonsterSpeed;
                    monster.stepsToBase++;

                    // если по результатам симуляции монстр дошел до базы - считаем его опасным
                    if (distToBase < 500 && monster.distanceToBase < 8000) {
                        monster.importance = (maxSimulationDepth - simulationDepth - 1) * 2500.;
                        bool needAdditionalUnit = (simulationDepth < 10) || monster.shieldLife;
                        monster.neededUnitsNb = static_cast<int>(std::ceil(static_cast<double>(monster.health) / (monster.stepsToBase * Entity::cUnitDamage))) + (needAdditionalUnit ? 1 : 0);

                        accountedDangeroudIdxs.insert(monster.id);
                        dangerousMonsters.push_back(monster);
                    }

                }
            }
        }

        // сохраняем монстров, которые не представляют угрозы, чтобы пофармить их если буду свободные юниты
        for (Entity& monster : monsters) {
            if (!accountedDangeroudIdxs.count(monster.id) && monster.distanceToBase < 8000) {
                monster.importance = (cMaxDist - dist(monster.coords, baseCoords)) * 2500. / cMaxDist;
                undangerousMonsters.push_back(monster);
            }
        }

        // cerr << "Sorting dangerous monsters, size: " << dangerousMonsters.size() << endl;
        // cerr << "Sorting undangerousMonsters monsters, size: " << dangerousMonsters.size() << endl;

        // сортировка по важности
        sort(dangerousMonsters.begin(), dangerousMonsters.end(), entityCompare);
        sort(undangerousMonsters.begin(), undangerousMonsters.end(), entityCompare);

        // cerr << "Creating actions" << endl;

        // формируем ходы для юнитов на основании важности монстра и количества юнитов, которое необходимо для его убийства
        Actions actions;
        actions.resize(3);
        for (Entity& monster : dangerousMonsters) {

            // cerr << "Monster with id " << monster.id << " has importance " << monster.importance
            //     << ", dist to base " << monster.distanceToBase << ", stepsToBase " << monster.stepsToBase
            //     << " and need units: " << monster.neededUnitsNb << endl;

            for (int neededUnitIdx = 0; neededUnitIdx < monster.neededUnitsNb; neededUnitIdx++) {
                // ищем ближайшего юнита к данному монстру
                double bestDistBetweenUnitAndMonster = cMaxDist, curDist;
                int bestOurUnitId = -1;
                for (Entity& ourUnit : our) {
                    curDist = dist(ourUnit.coords, monster.coords);
                    if (curDist < bestDistBetweenUnitAndMonster) {
                        bestDistBetweenUnitAndMonster = curDist;
                        bestOurUnitId = ourUnit.id;
                    }
                }

                // cerr << "\tBest our unit id" << bestOurUnitId << endl;

                // определяем индекс юнита (команды)
                int commandIdx = bestOurUnitId - (bestOurUnitId >= 3 ? 3 : 0);

                // если юнит ближе к базе чем монстр И дальность до монстра меньша дальности ветерка И дальность от базы < 7500 (обдумать эвристику)
                // то пытаемся выдувать монстра подальше
                double bestUnitDistToBase = dist(our[commandIdx].coords, baseCoords),
                       monsterDistToBase = dist(monster.coords, baseCoords);
                if (bestUnitDistToBase < monsterDistToBase + (1280./2.)
                    && bestDistBetweenUnitAndMonster < (1280. / 2.)
                    && monsterDistToBase < 7500.) {
                    actions[commandIdx] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                }
                // иначе идем с опережением монстра
                else {
                    actions[commandIdx] = { "MOVE", monster.coords.x + monster.vx, monster.coords.y + monster.vy };
                }

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

            // если есть неопасные монстры - идем фармить
            if (undangerousMonsters.size()) {

                for (const Entity& undangerousMonster: undangerousMonsters) {
                    // ищем ближайшего юнита к данному монстру
                    double bestDistBetweenUnitAndMonster = cMaxDist, curDist;
                    int bestOurUnitId = -1;

                    for (Entity& ourUnit : our) {
                        curDist = dist(ourUnit.coords, undangerousMonster.coords);
                        if (curDist < bestDistBetweenUnitAndMonster) {
                            bestDistBetweenUnitAndMonster = curDist;
                            bestOurUnitId = ourUnit.id;
                        }
                    }

                    // определяем индекс юнита (команды)
                    int commandIdx = bestOurUnitId - (bestOurUnitId >= 3 ? 3 : 0);

                    // добавляем команду на атаку ближайшим юнитом этого монстра
                    actions[commandIdx] = { "MOVE", undangerousMonster.coords.x, undangerousMonster.coords.y };

                    // удаляем выбранного юнита из доступных для хода
                    for (int i = 0; i < our.size(); i++) {
                        if (our[i].id == bestOurUnitId) {
                            our.erase(our.begin() + i);
                            break;
                        }
                    }

                    if (our.empty()) break;
                }
            }

            // cerr << "Creating wait actions for " << our.size() << " our units" << endl;
            for (int i = 0; i < our.size(); i++) {
                int commandIdx = our[i].id - (our[i].id >= 3 ? 3 : 0);
                actions[commandIdx] = { "MOVE", waitCoordsVec2[our.size() - 1][i].x, waitCoordsVec2[our.size() - 1][i].y };
            }
        }

        // если САМЫЙ ОПАСНЫЙ монстр на расстоянии 3 хода от базы И на нем нет щитка
        // то берем ближайшего героя к базе и принудительно ведем его точке на расстоянии 300 от базы под 45 градусов
        // как только этот юнит будет ближе всех остальных юнитов и монстров к базе - этот юнит юзает ветерок
        /*
        if (dangerousMonsters.size()) {

            double mostDangerMonsterDistToBase = dist(dangerousMonsters[0].coords, baseCoords);
            bool canBeWinded = !dangerousMonsters[0].shieldLife;

            // расстояние на котором считаем целесообразным применять ветерок
            double windActionsThreshold = 5 * 400.;
            if (mostDangerMonsterDistToBase < windActionsThreshold && canBeWinded) {
                // ищем ближайшего юнита к базе
                double bestUnitDist = cMaxDist, curDist;
                int bestOurUnitId = -1;

                for (Entity& ourUnit : safetyOur) {
                    curDist = dist(ourUnit.coords, baseCoords);
                    if (curDist < bestUnitDist) {
                        bestUnitDist = curDist;
                        bestOurUnitId = ourUnit.id;
                    }
                }

                // cerr << "Most dangerous monster: " << dangerousMonsters[0].id << ", best unit to WIND is " << bestOurUnitId << endl;

                // определяем индекс юнита (команды)
                int commandIdx = bestOurUnitId - (bestOurUnitId >= 3 ? 3 : 0);

                // если юнит уже ближе к базе чем монстр и расстояние до монстра < 1280, то дуем
                if ((bestUnitDist < mostDangerMonsterDistToBase) && (dist(dangerousMonsters[0].coords, safetyOur[commandIdx].coords) < 1280)) {
                    actions[commandIdx] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                }
                else {
                    // если юнит еще далеко, то пытаемся обогнать монстра
                    actions[commandIdx] = { "MOVE", dangerousMonsters[0].coords.x + dangerousMonsters[0].vx, dangerousMonsters[0].coords.y + dangerousMonsters[0].vy };
                }
            }
        }
        */

        return actions;
    }

    vector<vector<Coord>> getWaitCoords() {
        static const double defendRadiusCoef = 1.15;

        // cerr << "Base coords: " << baseCoords.x << " " << baseCoords.y << endl;

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
        for (const Action& action : entities.updateDangerousMonsters()) {
            cout << action.action << " " << action.val1;
            if (action.val2 != -1) cout << " " << action.val2;
            if (action.val3 != -1) cout << " " << action.val3;
            cout << endl;
        }
        cerr << "time per turn: " << std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count() << " ms\n";
    }
}