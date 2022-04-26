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
struct Coord {
    int x, y;
};
double dist(const Coord& c1, const Coord& c2) {
    return sqrt(pow(abs(c1.x - c2.x), 2) + pow(abs(c1.y - c2.y), 2));
}
const double cBaseRadius{ 5000 };
const Coord maxCoord{ 17630, 9000 };
const double cMaxDist = dist({ 0, 0 }, maxCoord);
const int cMonsterSpeed = 400;
const int cUnitDamage = 2;
int turnCnt = 0;
const int cMaxTurnIdx = 219;
const int cWindWidth = 1280;
const int cWindDistance = 2200;
const int cHalfWindWide = cWindWidth / 2;

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
struct Entity {
    int id, type, shieldLife, isControlled, health, vx, vy, nearBase, threatFor;
    Coord coords;

    double importance{ 0. };
    double distanceToBase{ 0. };
    int stepsToBase{ 0 };
    int neededUnitsNb{ 0 }; // МИНИМАЛЬНОЕ количество юнитов, требующееся для убийства монстра
    Coord simulatedCoords;

    void read() {
        cin >> id >> type >> coords.x >> coords.y >> shieldLife >> isControlled >> health >> vx >> vy >> nearBase >> threatFor; cin.ignore();
    }
};
bool entityCompare(const Entity& left, const Entity& right) {
    return (left.importance > right.importance);
}
int getIdxOfNearestUnits(const Coord& targetCoords, const vector<Entity>& potentialEntities) {
    double bestDistBetweenUnitAndTarger = cMaxDist, curDist;
    int bestUnitId = -1;
    for (const Entity& potentUnit : potentialEntities) {
        curDist = dist(potentUnit.coords, targetCoords);
        if (curDist < bestDistBetweenUnitAndTarger) {
            bestDistBetweenUnitAndTarger = curDist;
            bestUnitId = potentUnit.id;
        }
    }
    return bestUnitId;
}
int convertToArrayIdx(const int someId) {
    return (someId - (someId >= 3 ? 3 : 0));
}
void eraseFromVecUnitId(int id, vector<Entity>& vec) {
    if (vec.empty()) return;
    if (vec[0].id > 2) id += 3;
    for (int i = 0; i < vec.size(); i++) {
        if (vec[i].id == id) {
            vec.erase(vec.begin() + i);
            return;
        }
    }
}
int getWindCastAllowRadius() {
    return 5000 + 1000 * bases.ourBase.mana / 20;
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

        set<int> accountedInDangeroudIdxs; // учтенные в расчете дистанции до базы (попадают в этот сет, когда расстояние до базы <= 400)
        for (int i = 0; i < enemies.size(); i++) {
            double curStepDist = dist(enemies[i].coords, baseCoords),
                   nextStepDist = dist({ enemies[i].coords.x + enemies[i].vx, enemies[i].coords.y + enemies[i].vy }, baseCoords);

            // если враг на нашей половине и идет В СТОРОНУ нашей базы
            if ((curStepDist < cMaxDist / 2) && (nextStepDist < curStepDist)) {
                // cerr << "Enemy " << enemies[i].id << " is danger for our base" << endl;
                monsters.push_back(enemies[i]);
            }
        }
        for (int simulationDepth = 0; simulationDepth < maxSimulationDepth; simulationDepth++) {
            for (Entity& monster : monsters) {
                monster.simulatedCoords.x += monster.vx;
                monster.simulatedCoords.y += monster.vy;

                double distToBase = dist(monster.simulatedCoords, baseCoords);

                // меняеем вектор скорости, когда монстр заходит в радиус базы
                if (distToBase < cBaseRadius)
                {
                    int dBaseX = baseCoords.x - monster.simulatedCoords.x,
                        dBaseY = baseCoords.y - monster.simulatedCoords.y;
                    monster.vx = ceil(cMonsterSpeed * dBaseX / distToBase);
                    monster.vy = ceil(cMonsterSpeed * dBaseY / distToBase);

                    // cerr << "On simulation " << simulationDepth << " monster " << monster.id << " speed: " << monster.vx << " " << monster.vy << endl;
                }

                // отдельно обрабатываем пушеров врага
                // достаточное условие реакции на такого юнита - достижение им радиуса базы
                if (monster.type == Enemy && distToBase < cBaseRadius && !accountedInDangeroudIdxs.count(monster.id)) {
                    monster.importance = (cMaxDist / 2. - dist(monster.coords, baseCoords)) * 3500.;
                    monster.neededUnitsNb = 1;

                    // cerr << "Enemy " << monster.id << " importance: " << monster.importance << endl;
                    accountedInDangeroudIdxs.insert(monster.id);
                    dangerousMonsters.push_back(monster);
                }
                // остальные кейсы - обычные монстры
                // добавляем дистанцию к общей дистанции до базы за тот ход, который симулируем
                // если монстр у базы, то для него в последующих симуляциях не надо считать дистанцию
                else if (!accountedInDangeroudIdxs.count(monster.id)) {
                    monster.distanceToBase += cMonsterSpeed;
                    monster.stepsToBase++;

                    // если по результатам симуляции монстр дошел до базы - считаем его опасным
                    if (distToBase < 500 && monster.distanceToBase < 9000) {
                        monster.importance = (maxSimulationDepth - simulationDepth - 1) * 2500.;
                        bool needAdditionalUnit = (simulationDepth < 8) || monster.shieldLife;
                        monster.neededUnitsNb = static_cast<int>(std::ceil(static_cast<double>(monster.health) / (monster.stepsToBase * cUnitDamage))) + (needAdditionalUnit ? 1 : 0);

                        // err << "Monster " << monster.id << " importance: " << monster.importance << " neededUnits: " << monster.neededUnitsNb << endl;

                        accountedInDangeroudIdxs.insert(monster.id);
                        dangerousMonsters.push_back(monster);
                    }
                }
            }
        }

        // сохраняем монстров, которые не представляют угрозы, чтобы пофармить их если будут свободные юниты
        for (Entity& monster : monsters) {
            // cerr << "Monster with id " << monster.id << " is accounted in dangerous: " << accountedInDangeroudIdxs.count(monster.id) << endl;
            if (!accountedInDangeroudIdxs.count(monster.id) && dist(monster.coords, baseCoords) < 8500.) {
                monster.importance = (cMaxDist - dist(monster.coords, baseCoords)) * 2500. / cMaxDist;
                undangerousMonsters.push_back(monster);
            }
        }

        // сортировка по важности
        sort(dangerousMonsters.begin(), dangerousMonsters.end(), entityCompare);
        sort(undangerousMonsters.begin(), undangerousMonsters.end(), entityCompare);
        // cerr << "Sorting completed" << endl;
        // cerr << "Wind Cast Allow Radius: " << getWindCastAllowRadius() << endl;
        // cerr << "Dangerous:" << endl;
        // for (const Entity& monster : dangerousMonsters) {
        //     cerr << "\t\tEntity " << monster.id << ", importance: " << monster.importance << endl;
        // }

        // формируем ходы для юнитов на основании важности монстра и количества юнитов, которое необходимо для его убийства
        Actions actions;
        actions.resize(3);
        for (Entity& monster : dangerousMonsters) {

            // cerr << "Monster with id " << monster.id << " has importance " << monster.importance
            //     << ", dist to base " << monster.distanceToBase << ", stepsToBase " << monster.stepsToBase
            //     << " and need units: " << monster.neededUnitsNb << endl;

            for (int neededUnitIdx = 0; neededUnitIdx < monster.neededUnitsNb; neededUnitIdx++) {
                // ищем ближайшего юнита к данному монстру
                int bestOurUnitId = convertToArrayIdx(getIdxOfNearestUnits(monster.coords, our));
                // cerr << "\tBest our unit for dangerous entity " << monster.id << " is " << bestOurUnitId << endl;

                // если юнит ближе к базе чем монстр И дальность до монстра меньша дальности ветерка И дальность от базы < 7500 (обдумать эвристику)
                // то пытаемся выдувать монстра подальше
                double bestUnitDistToBase = dist(our[bestOurUnitId].coords, baseCoords),
                       monsterDistToBase = dist(monster.coords, baseCoords),
                       distBetweenUnitAndMonster = dist(monster.coords, our[bestOurUnitId].coords);

                // отдельно обрабатываем кейс с контром врага
                if (monster.type == Enemy && bases.ourBase.mana > 10 && !monster.shieldLife) {
                    // cerr << "Monster type Enemy" << endl;
                    // cerr << "\tEnemy dangerous entity, bestUnitDistToBase:" << bestUnitDistToBase << ", monsterDistToBase:" << monsterDistToBase << endl;
                    // cerr << "\t\tdistBetweenUnitAndMonster:" << distBetweenUnitAndMonster << endl;
                    if (distBetweenUnitAndMonster < cHalfWindWide * 1.5) {
                        actions[bestOurUnitId] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                    }
                    // иначе идем с опережением врага
                    else {
                        actions[bestOurUnitId] = { "MOVE", monster.coords.x + monster.vx, monster.coords.y + monster.vy };
                    }
                }
                // обрабатываем монстров
                else if (bestUnitDistToBase < monsterDistToBase + cHalfWindWide // если наш юнит БЛИЖЕ к базе чем монстр на половину каста ветерка
                    && distBetweenUnitAndMonster < cHalfWindWide
                    && monsterDistToBase < getWindCastAllowRadius()
                    && !monster.shieldLife) {
                    // cerr << "Monster type Monster, can cast WIND" << endl;
                    actions[bestOurUnitId] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                }
                // иначе идем с опережением монстра
                else {
                    // cerr << "Monster type Monster, go forward him: MOVE " << monster.coords.x + monster.vx << ", " << monster.coords.y + monster.vy << endl;
                    actions[bestOurUnitId] = { "MOVE", monster.coords.x + monster.vx, monster.coords.y + monster.vy };
                }

                // удаляем выбранного юнита из доступных для хода
                eraseFromVecUnitId(bestOurUnitId, our);

                if (our.empty()) break;
            }
            if (our.empty()) break;
        }
        // cerr << "Dangerous processed" << endl;

        if (our.size()) {

            // если есть неопасные монстры - идем фармить
            if (undangerousMonsters.size()) {

                for (const Entity& undangerousMonster: undangerousMonsters) {
                    // ищем ближайшего юнита к данному монстру
                    int bestOurUnitId = convertToArrayIdx(getIdxOfNearestUnits(undangerousMonster.coords, our));

                    // добавляем команду на атаку ближайшим юнитом этого монстра
                    actions[bestOurUnitId] = { "MOVE", undangerousMonster.coords.x + undangerousMonster.vx, undangerousMonster.coords.y + undangerousMonster.vy };

                    /*
                    // если юнит ближе к базе чем монстр И дальность до монстра меньша дальности ветерка И дальность от базы < 7500 (обдумать эвристику)
                    // то пытаемся выдувать монстра подальше
                    double bestUnitDistToBase = dist(our[bestOurUnitId].coords, baseCoords),
                        monsterDistToBase = dist(undangerousMonster.coords, baseCoords),
                        distBetweenUnitAndMonster = dist(undangerousMonster.coords, our[bestOurUnitId].coords);
                    if (bestUnitDistToBase < monsterDistToBase + cHalfWindWide
                        && distBetweenUnitAndMonster < cHalfWindWide
                        && monsterDistToBase < 6000.) {
                        actions[bestOurUnitId] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                    }
                    // иначе идем с опережением монстра
                    else {
                        actions[bestOurUnitId] = { "MOVE", undangerousMonster.coords.x + undangerousMonster.vx, undangerousMonster.coords.y + undangerousMonster.vy };
                    }
                    */

                    eraseFromVecUnitId(bestOurUnitId, our);
                    if (our.empty()) break;
                }
            }
            // cerr << "Undangerous processed" << endl;

            // cerr << "Creating wait actions for " << our.size() << " our units" << endl;
            for (int i = 0; i < our.size(); i++) {
                actions[convertToArrayIdx(our[i].id)] = { "MOVE", waitCoordsVec2[our.size() - 1][i].x, waitCoordsVec2[our.size() - 1][i].y };
            }
        }
        // cerr << "Actions ready" << endl;

        return actions;
    }

    vector<vector<Coord>> getWaitCoords() {
        static const double defendRadiusCoef = 1.3;

        // cerr << "Base coords: " << baseCoords.x << " " << baseCoords.y << endl;

        return {
            vector<Coord> {
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(45. * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(45. * M_PI / 180.)))) }
            },
            vector<Coord> {
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(20. * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(20. * M_PI / 180.)))) },
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(70. * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(70. * M_PI / 180.)))) }
            },
            vector<Coord> {
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(20. * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(20. * M_PI / 180.)))) },
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(45. * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(45. * M_PI / 180.)))) },
                Coord{  abs(baseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(70. * M_PI / 180.)))),
                        abs(baseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(70. * M_PI / 180.)))) }
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
            if (action.val2 != -1 || action.action == "MOVE") cout << " " << action.val2;
            if (action.val3 != -1) cout << " " << action.val3;
            cout << endl;
        }
        turnCnt++;
        cerr << "time per turn: " << std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count() << " ms\n";
    }
}