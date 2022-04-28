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
const int cHalfWindWidth = cWindWidth / 2;
const int cWindDistance = 2200;
const int cHalfWindDistance = cWindDistance / 2;
const int cMaxSimulationDepth = 35;
const double cHeroVisibleAreaRadius = 2200.;
const double cDistanceThresholdWhenEnemyReachBase = 500.; // расстояние, на котором считается, что монстр достиг базы
const double cDangerousDistanceThreshold = 9000.; // порог криволинейного расстояния до нашей базы, после которого монстр считается ОПАСНЫМ для нас
const double cImportanceStep = 2500.; // важность монстра, уменьшающаяся с каждым ходом на величину шага

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
    double distanceToReachBase{ 0. };
    int stepsToBase{ 0 };
    int neededUnitsNb{ 0 }; // МИНИМАЛЬНОЕ количество юнитов, требующееся для убийства монстра
    Coord simulatedCoords;
    double minDistanceToBase{ cMaxDist }; // минимальное расстояние до базы, которого достигнет монстр за все симуляции (положительное для нашей базы, отрицательное - для вражеской)

    bool needShield = false, needWind = false;

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
    return 5000 + 1000 * bases.ourBase.mana / 40;
}
bool isEntityInCoord(const Entity& entity, const Coord& coord) {
    return dist(entity.coords, coord) < 2;
}
double getEntityAlphaToCoords(const Entity& entity, const Coord& coord) {
    double dx = abs(coord.x - entity.coords.x),
        dy = abs(coord.y - entity.coords.y),
        _dist = dist(coord, entity.coords);
    return 0;
}

enum UNIT_TYPES {
    Defender,
    Attacker,
    Farmer
};

struct Entities {
    vector<Entity> our, enemies, monsters;
    Coord ourBaseCoords, enemyBaseCoords;
    int entitiesNb;
    bool AtackerHavePushMonsters = false;

    Coord minimalAttackCoord;
    vector<Coord> attackCoordsVec, farmCoordVec;
    int farmerCurrentTargetPos = -1,
        attackerCurrentTargetPos = -1,
        defenderCurrentTargetPos = -1;

    void readBaseCoords() {
        cin >> ourBaseCoords.x >> ourBaseCoords.y; cin.ignore();
        enemyBaseCoords = { maxCoord.x - ourBaseCoords.x, maxCoord.y - ourBaseCoords.y };
        minimalAttackCoord = getMinimalAttackCoord();
        attackCoordsVec = getAttackDefaultCoords();
        farmCoordVec = getFarmDefaultCoords();
    }

    void read() {
        monsters.clear(); our.clear(); enemies.clear();
        cin >> entitiesNb; cin.ignore();
        for (int entityId = 0; entityId < entitiesNb; entityId++) {
            Entity tempEntity;
            tempEntity.read();
            switch (tempEntity.type) {
            case Monster: monsters.push_back(tempEntity); break;
            case Our:     our.push_back(tempEntity); break;
            case Enemy:   enemies.push_back(tempEntity); break;
            default: break;
            }
        }
    }

    Actions prepairActions() {

        vector<Entity> ourDangerousMonsters, enemyDangerousMonsters, monstersToFarmingAndEnemyUnits,
            safetyOur(our.begin(), our.end());
        set<int> ourDangerousAccountedMonsters, // учтенные в расчете дистанции до базы (попадают в этот сет, когда расстояние до базы <= 400)
            enemyDangerousAccountedMonsters,
            accountedEnemyUnits;

        for (Entity& monster : monsters) {
            monster.simulatedCoords = monster.coords;
        }

        // заносим опасных вражеских юнитов в вектор с монстрами
        {
            for (int i = 0; i < enemies.size(); i++) {
                double curStepDist = dist(enemies[i].coords, ourBaseCoords),
                    nextStepDist = dist({ enemies[i].coords.x + enemies[i].vx, enemies[i].coords.y + enemies[i].vy }, ourBaseCoords);

                // если враг на нашей половине и идет В СТОРОНУ нашей базы
                if ((curStepDist < cMaxDist / 2) && (nextStepDist < curStepDist)) {
                    // cerr << "Enemy " << enemies[i].id << " is danger for our base" << endl;
                    monsters.push_back(enemies[i]);
                }
            }
        }

        // симуляция движений видимых сущностей
        {
            double curDistToOurBase, curDistToEnemyBase;
            for (int simulationDepth = 0; simulationDepth < cMaxSimulationDepth; simulationDepth++) {
                for (Entity& monster : monsters) {

                    // симулируем координаты монстра и учитываем его дистанции
                    {
                        monster.simulatedCoords.x += monster.vx;
                        monster.simulatedCoords.y += monster.vy;

                        // дистанция до баз на i-м шаге симуляция
                        curDistToOurBase = dist(monster.simulatedCoords, ourBaseCoords);
                        curDistToEnemyBase = dist(monster.simulatedCoords, enemyBaseCoords);

                        // сохраняем минимальные дистанции за все симуляции
                        if (curDistToOurBase < abs(monster.minDistanceToBase)) monster.minDistanceToBase = curDistToOurBase;
                        if (curDistToEnemyBase < abs(monster.minDistanceToBase)) monster.minDistanceToBase = -curDistToEnemyBase;
                    }

                    // меняеем вектор скорости, когда монстр заходит в радиус базы
                    {
                        if (curDistToOurBase < cBaseRadius)
                        {
                            int dBaseX = ourBaseCoords.x - monster.simulatedCoords.x, // важно чтобы значения могли быть отрицательным
                                dBaseY = ourBaseCoords.y - monster.simulatedCoords.y; // для правильных значений компонент скорости
                            monster.vx = ceil(cMonsterSpeed * dBaseX / curDistToOurBase);
                            monster.vy = ceil(cMonsterSpeed * dBaseY / curDistToOurBase);
                        }
                        if (curDistToEnemyBase < cBaseRadius)
                        {
                            int dBaseX = enemyBaseCoords.x - monster.simulatedCoords.x, // важно чтобы значения могли быть отрицательным
                                dBaseY = enemyBaseCoords.y - monster.simulatedCoords.y; // для правильных значений компонент скорости
                            monster.vx = ceil(cMonsterSpeed * dBaseX / curDistToEnemyBase);
                            monster.vy = ceil(cMonsterSpeed * dBaseY / curDistToEnemyBase);
                        }
                        // cerr << "On simulation " << simulationDepth << " monster " << monster.id << " speed: " << monster.vx << " " << monster.vy << endl;
                    }

                    // проверяем наличие вражеских пушеров
                    {
                        // достаточное условие реакции на такого юнита - достижение им радиуса базы
                        if (monster.type == Enemy
                            && curDistToOurBase < cBaseRadius
                            && !accountedEnemyUnits.count(monster.id)) {
                            monster.importance = (cMaxDist / 2. - dist(monster.coords, ourBaseCoords)) * 3500.;
                            monster.neededUnitsNb = 1;

                            // cerr << "Enemy " << monster.id << " importance: " << monster.importance << endl;
                            accountedEnemyUnits.insert(monster.id);
                            monstersToFarmingAndEnemyUnits.push_back(monster);
                        }
                    }

                    // учет положений и состояний пауков
                    {
                        // добавляем дистанцию к общей дистанции до базы за тот ход, который симулируем
                        if (!ourDangerousAccountedMonsters.count(monster.id)
                            && !enemyDangerousAccountedMonsters.count(monster.id)) {
                            monster.distanceToReachBase += cMonsterSpeed;
                            monster.stepsToBase++;

                            // если по результатам симуляции монстр дошел до базы - считаем его опасным

                            // ДЛЯ НАШЕЙ БАЗЫ
                            if (curDistToOurBase < cDistanceThresholdWhenEnemyReachBase
                                && monster.distanceToReachBase < cDangerousDistanceThreshold) {
                                monster.importance = (cMaxSimulationDepth - simulationDepth - 1) * cImportanceStep;

                                ourDangerousAccountedMonsters.insert(monster.id);
                                ourDangerousMonsters.push_back(monster);
                            }
                            // ДЛЯ ВРАЖЕСКОЙ БАЗЫ
                            // флаг, означающий, что монстр сам атакует базу и ему можно будет помочь
                            bool selfAttackedEnemyBase =
                                (curDistToEnemyBase < cDistanceThresholdWhenEnemyReachBase) // если монстр все же достигнет вражеской базы
                                && (monster.distanceToReachBase < cDangerousDistanceThreshold) // и если это будет в обозримом будущем
                                && (monster.stepsToBase < 20); // и щиток у монстра кончится раньше, чем он достигнет базы, чтобы его можно было задуть
                            // cerr << "Entity " << monster.id << " is selfAttackedEnemyBase: " << selfAttackedEnemyBase << endl;
                            // с этим флагом расчет важности монстра производится по формуле, зависящей от номера симуляции
                            if (selfAttackedEnemyBase) {
                                monster.importance = (cMaxSimulationDepth - simulationDepth - 1) * cImportanceStep;
                                monster.needShield = true;
                                enemyDangerousAccountedMonsters.insert(monster.id);
                                enemyDangerousMonsters.push_back(monster);
                            }
                            // флаг, означающий, что монстра можно сдуть с ТЕКУЩЕЙ позиции в радиус
                            bool canBeMovedToEnemyBase =
                                (simulationDepth <= 2)
                                && (curDistToEnemyBase < 6700.)
                                && (!monster.shieldLife);
                            // cerr << "Entity " << monster.id << " is canBeMovedToEnemyBase: " << selfAttackedEnemyBase << endl;
                            if (canBeMovedToEnemyBase) {
                                double distanceBetweenAttackerAndMonster = dist(monster.simulatedCoords, our[Attacker].coords);
                                monster.importance = (5000. - distanceBetweenAttackerAndMonster) * cImportanceStep;
                                monster.needWind = true;
                                enemyDangerousAccountedMonsters.insert(monster.id);
                                enemyDangerousMonsters.push_back(monster);
                            }
                        }
                    }
                }
            }
        }

        // учет монстров, которых можно пофармить
        {
            for (Entity& monster : monsters) {
                if (!ourDangerousAccountedMonsters.count(monster.id) // если монстр не опасный для нас
                    && !ourDangerousAccountedMonsters.count(monster.id) // и монстр не опасный для врага
                    && monster.minDistanceToBase < 8500. // считаем пригодным для фарма, если он проходит от НАШЕЙ базы на расстоянии <8500
                    && monster.minDistanceToBase > -1000. // фармим с небольшим запасом по дистанции от нашей половины карты, что юнит не уходил далеко
                    && monster.type == EntityType::Monster) {
                    // важность монстра считается по его ТЕКУЩЕМУ положению
                    monster.importance = (cMaxDist - dist(monster.coords, ourBaseCoords)) * 2500. / cMaxDist;
                    monstersToFarmingAndEnemyUnits.push_back(monster);
                }
            }
        }

        // сортировка по важности
        {
            sort(ourDangerousMonsters.begin(), ourDangerousMonsters.end(), entityCompare);
            sort(enemyDangerousMonsters.begin(), enemyDangerousMonsters.end(), entityCompare);
            sort(monstersToFarmingAndEnemyUnits.begin(), monstersToFarmingAndEnemyUnits.end(), entityCompare);

            // cerr << "Sorting completed" << endl;
            // cerr << "Wind Cast Allow Radius: " << getWindCastAllowRadius() << endl;

            /*
            cerr << "Our Dangerous:" << endl;
            for (const Entity& monster : ourDangerousMonsters) {
                cerr << "\t\tEntity " << monster.id << ", importance: " << monster.importance << endl;
            }
            */

            /*
            cerr << "Enemy Dangerous:" << endl;
            for (const Entity& monster : enemyDangerousMonsters) {
                cerr << "\t\tEntity " << monster.id << ", importance: " << monster.importance << endl;
            }
            */
        }

        // формируем ходы
        Actions actions;
        actions.resize(3);
        {
        // если нет целей для атакера и дефендера, то они занимаются фармом
        bool isDefenderBusy = !ourDangerousMonsters.empty(),
            isAttackerBusy = !enemyDangerousMonsters.empty(),
            // isFarmerBusy = !monstersToFarmingAndEnemyUnits.empty();
            isFarmerBusy = ourDangerousMonsters.size() > 1;

        // формируем ход для ATTACKER
        {
            double attackerDistToBase = dist(our[Attacker].coords, enemyBaseCoords);

            cerr << "Attacker havePushMonsters: " << AtackerHavePushMonsters << endl;
            cerr << "enemyDangerousMonsters size: " << enemyDangerousMonsters.size() << endl;

            // если список пауков для врага пустой
            if (enemyDangerousMonsters.empty()) {
                cerr << "enemyDangerousMonsters is empty" << endl;

                // надо выбрать куда идти
                Coord attackerTargerCoord;
                bool isAttackerInBestPos = isEntityInCoord(our[Attacker], minimalAttackCoord);
                bool isAttackerInWatchPos0 = isEntityInCoord(our[Attacker], attackCoordsVec[0]);
                bool isAttackerInWatchPos1 = isEntityInCoord(our[Attacker], attackCoordsVec[1]);

                // если был ранее задуваемый - идем вглубь
                // как только доходим до глубины - идем на обзорные позиции
                if (AtackerHavePushMonsters && !isAttackerInBestPos) {
                    attackerCurrentTargetPos = -1;
                    attackerTargerCoord = minimalAttackCoord;
                    cerr << "attackerTargerCoord = minimalAttackCoord" << endl;
                }
                else {

                    if (isAttackerInBestPos) {
                        AtackerHavePushMonsters = false;
                    }

                    if (attackerCurrentTargetPos == -1) {
                        attackerCurrentTargetPos = 0;
                    }
                    if (isAttackerInWatchPos0) {
                        attackerCurrentTargetPos = 1;
                    }
                    else if (isAttackerInWatchPos1) {
                        attackerCurrentTargetPos = 0;
                    }
                    cerr << "attackerCurrentTargetPos = " << attackerCurrentTargetPos << endl;
                    cerr << "attackerTargerCoord = attackCoordsVec[" << attackerCurrentTargetPos << "]" << endl;
                    attackerTargerCoord = attackCoordsVec[attackerCurrentTargetPos];
                }
                actions[Attacker] = { "MOVE", attackerTargerCoord.x, attackerTargerCoord.y };
            }
            else {
                attackerCurrentTargetPos = -1;

                // самый опасный монстр
                const Entity& mostDangerousEnemyMonster = enemyDangerousMonsters.front();

                double monsterDistToEnemyBase = dist(mostDangerousEnemyMonster.coords, enemyBaseCoords),
                    distBetweenUnitAndMonster = dist(mostDangerousEnemyMonster.coords, our[Attacker].coords);

                cerr << "Attacker target: " << mostDangerousEnemyMonster.id << endl;

                // наилучший монстр близко и его можно сдуть
                if (monsterDistToEnemyBase < attackerDistToBase // если монстр БЛИЖЕ к базе чем ATTACKER
                    && distBetweenUnitAndMonster < cHalfWindDistance // и ветерок заденет монстра
                    && !mostDangerousEnemyMonster.shieldLife // нет смысла сдувать монстров с щитом
                    && bases.ourBase.mana >= 10) { // проверяем наличие маны на ветерок
                    cerr << "Attacker can wind nearest monster" << endl;
                    actions[Attacker] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                    AtackerHavePushMonsters = true;
                    bases.ourBase.mana -= 10;
                }
                else {
                    // если монстр НЕДАЛЕКО от вражеского круга
                    // и если у него нет щитка или если щиток кончится тогда, когда расстояния монстра до базы будет около 1000
                    // то догоняем его со спины
                    if (monsterDistToEnemyBase < 7500 // cBaseRadius + 7500
                        && (!mostDangerousEnemyMonster.shieldLife
                            || ((monsterDistToEnemyBase - 300.) / cMonsterSpeed > mostDangerousEnemyMonster.shieldLife))) {
                        cerr << "Attacker will go behind the back to monster " << mostDangerousEnemyMonster.id << endl;
                        actions[Attacker] = { "MOVE",
                            mostDangerousEnemyMonster.coords.x - static_cast<int>(ceil(mostDangerousEnemyMonster.vx * 1.2)),
                            mostDangerousEnemyMonster.coords.y - static_cast<int>(ceil(mostDangerousEnemyMonster.vy * 1.2)) };
                        AtackerHavePushMonsters = true;
                    }
                    else {
                        cerr << "Available monsters is bad" << endl;
                        AtackerHavePushMonsters = false;

                        // надо выбрать куда идти
                        Coord attackerTargerCoord;
                        bool isAttackerInWatchPos0 = isEntityInCoord(our[Attacker], attackCoordsVec[0]);
                        bool isAttackerInWatchPos1 = isEntityInCoord(our[Attacker], attackCoordsVec[1]);

                        if (attackerCurrentTargetPos == -1) {
                            attackerCurrentTargetPos = 0;
                        }
                        if (isAttackerInWatchPos0) {
                            attackerCurrentTargetPos = 1;
                        }
                        else if (isAttackerInWatchPos1) {
                            attackerCurrentTargetPos = 0;
                        }
                        attackerTargerCoord = attackCoordsVec[attackerCurrentTargetPos];
                        actions[Attacker] = { "MOVE", attackerTargerCoord.x, attackerTargerCoord.y };
                    }
                }

                /*
                // пытаемся навесить щит
                if (mostDangerousEnemyMonster.needShield
                    && distBetweenUnitAndMonster < 2200.
                    && !mostDangerousEnemyMonster.shieldLife) {
                    actions[Attacker] = { "SPELL SHIELD", mostDangerousEnemyMonster.id };
                }
                // пытаемся задувать монстра вглубь базы врага
                else if (mostDangerousEnemyMonster.needWind
                    && monsterDistToEnemyBase < attackerDistToBase // если монстр БЛИЖЕ к базе чем ATTACKER
                    && distBetweenUnitAndMonster < cHalfWindDistance // и ветерок заденет монстра
                    && !mostDangerousEnemyMonster.shieldLife) { // нет смысла сдувать монстров с щитом
                        actions[Attacker] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                        our[Attacker].havePushMonsters = true;
                }
                // иначе идем монстру за спину, чтобы потом его сдуть
                else {
                    actions[Attacker] = { "MOVE",
                        mostDangerousEnemyMonster.coords.x - static_cast<int>(ceil(mostDangerousEnemyMonster.vx * 1.5)),
                        mostDangerousEnemyMonster.coords.y - static_cast<int>(ceil(mostDangerousEnemyMonster.vy * 1.5)) };
                }
                */
            }
        }

        // формируем ход для FARMER
        {
            /*
            // если есть цели для фарминга
            if (isFarmerBusy) {
                farmerCurrentTargetPos = -1;
                // наилучший монстр для фарминга или враг для дефа
                const Entity& bestEntity = monstersToFarmingAndEnemyUnits.front();

                // если юнит ближе к базе чем монстр И дальность до монстра меньша дальности ветерка И дальность от базы < 7500 (обдумать эвристику)
                // то пытаемся выдувать монстра подальше
                double farmerDistToBase = dist(our[Farmer].coords, ourBaseCoords),
                    bestEntityToBase = dist(bestEntity.coords, ourBaseCoords),
                    distBetweenOurAndEntity = dist(bestEntity.coords, our[Farmer].coords);

                // отдельно обрабатываем кейс с контром врага
                if (bestEntity.type == Enemy && bases.ourBase.mana > 10 && !bestEntity.shieldLife) {
                    if (distBetweenOurAndEntity < cHalfWindWidth) {
                        actions[Farmer] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                    }
                    // иначе идем с опережением врага
                    else {
                        actions[Farmer] = { "MOVE", bestEntity.coords.x + bestEntity.vx, bestEntity.coords.y + bestEntity.vy };
                    }
                }
                // если это паук или нет маны на деф вражеского пушера
                else {
                    actions[Farmer] = { "MOVE", bestEntity.coords.x + bestEntity.vx, bestEntity.coords.y + bestEntity.vy };
                }
            }
            // если некого фармить - идем на дефолтную позицию ожидания
            // важно запонмить таргет фармера, чтобы дефендер знал куда идти в случае бездействия
            else {
                if (farmerCurrentTargetPos == -1) {
                    farmerCurrentTargetPos = 0;
                }
                if (   (our[Farmer].coords.x == farmCoordVec[0].x)
                    && (our[Farmer].coords.y == farmCoordVec[0].y)) {
                    farmerCurrentTargetPos = 1;
                }
                else if ((our[Farmer].coords.x == farmCoordVec[1].x)
                    && (our[Farmer].coords.y == farmCoordVec[1].y)) {
                    farmerCurrentTargetPos = 0;
                }
                actions[Farmer] = { "MOVE", farmCoordVec[farmerCurrentTargetPos].x, farmCoordVec[farmerCurrentTargetPos].y };
            }
            */

            if (isFarmerBusy)
            {
                farmerCurrentTargetPos = -1;
                // 2-й по опасности монстр
                const Entity& dangerousMonster = ourDangerousMonsters[1];

                // если Farmer ближе к базе чем монстр И дальность до монстра меньша дальности ветерка И дальность от базы < 7500 (обдумать эвристику)
                // то пытаемся выдувать монстра подальше
                double farmerDistToBase = dist(our[Farmer].coords, ourBaseCoords),
                    monsterDistToBase = dist(dangerousMonster.coords, ourBaseCoords),
                    distBetweenUnitAndMonster = dist(dangerousMonster.coords, our[Farmer].coords);

                if (farmerDistToBase < monsterDistToBase + cHalfWindWidth // если Farmer БЛИЖЕ к базе чем монстр на половину каста ветерка
                    && distBetweenUnitAndMonster < cHalfWindWidth // и ветерок заденет монстра
                    && monsterDistToBase < cBaseRadius
                    && !dangerousMonster.shieldLife) { // нет смысла сдувать монстров с щитом
                    // cerr << "Monster type Monster, can cast WIND" << endl;
                    actions[Farmer] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                }
                // иначе идем с опережением монстра
                else {
                    // cerr << "Monster type Monster, go forward him: MOVE " << dangerousMonster.coords.x + dangerousMonster.vx << ", " << dangerousMonster.coords.y + dangerousMonster.vy << endl;
                    actions[Farmer] = { "MOVE", dangerousMonster.coords.x + dangerousMonster.vx, dangerousMonster.coords.y + dangerousMonster.vy };
                }
            }
            // идем на противоположную позицию фарма от фермера
            // либо на 1-ю дефолтную позицию для фарма
            else {
                if (farmerCurrentTargetPos == -1) {
                    farmerCurrentTargetPos = 0;
                }
                if ((our[Farmer].coords.x == farmCoordVec[0].x)
                    && (our[Farmer].coords.y == farmCoordVec[0].y)) {
                    farmerCurrentTargetPos = 1;
                }
                else if ((our[Farmer].coords.x == farmCoordVec[1].x)
                    && (our[Farmer].coords.y == farmCoordVec[1].y)) {
                    farmerCurrentTargetPos = 0;
                }
                actions[Farmer] = { "MOVE", farmCoordVec[farmerCurrentTargetPos].x, farmCoordVec[farmerCurrentTargetPos].y };
            }
        }

        // формируем ход для DEFENDER
        {
            if (isDefenderBusy)
            {
                defenderCurrentTargetPos = -1;
                // самый опасный монстр
                const Entity& mostDangerousMonster = ourDangerousMonsters.front();

                // если DEFENDER ближе к базе чем монстр И дальность до монстра меньша дальности ветерка И дальность от базы < 7500 (обдумать эвристику)
                // то пытаемся выдувать монстра подальше
                double defenderDistToBase = dist(our[Defender].coords, ourBaseCoords),
                    monsterDistToBase = dist(mostDangerousMonster.coords, ourBaseCoords),
                    distBetweenUnitAndMonster = dist(mostDangerousMonster.coords, our[Defender].coords);

                if (defenderDistToBase < monsterDistToBase + cHalfWindWidth // если DEFENDER БЛИЖЕ к базе чем монстр на половину каста ветерка
                    && distBetweenUnitAndMonster < cHalfWindWidth // и ветерок заденет монстра
                    && monsterDistToBase < cBaseRadius
                    && !mostDangerousMonster.shieldLife) { // нет смысла сдувать монстров с щитом
                    // cerr << "Monster type Monster, can cast WIND" << endl;
                    actions[Defender] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                }
                // иначе идем с опережением монстра
                else {
                    // cerr << "Monster type Monster, go forward him: MOVE " << mostDangerousMonster.coords.x + mostDangerousMonster.vx << ", " << mostDangerousMonster.coords.y + mostDangerousMonster.vy << endl;
                    actions[Defender] = { "MOVE", mostDangerousMonster.coords.x + mostDangerousMonster.vx, mostDangerousMonster.coords.y + mostDangerousMonster.vy };
                }
            }
            // если дефендеру нечего делать - присоединяемся к фермеру
            // else if (monstersToFarmingAndEnemyUnits.size() >= 2) {
            //     // монстр-таргет для фарминга сделаем ближайшим к дефендеру
            //     int bestTargetToFarmIdx = -1;
            //     double bestTargetDist = cMaxDist;
            //     for (int i = 1; i < monstersToFarmingAndEnemyUnits.size(); i++) {
            //         double distBetweenDefenderAndMonster = dist(monstersToFarmingAndEnemyUnits[i].coords, our[Defender].coords);
            //         if (distBetweenDefenderAndMonster < bestTargetDist) {
            //             bestTargetDist = distBetweenDefenderAndMonster;
            //             bestTargetToFarmIdx = i;
            //         }
            //     }
            //     // после определения лучшего таргета - атакуем
            //     const Entity& bestTargetMonster = monstersToFarmingAndEnemyUnits[bestTargetToFarmIdx];
            //     actions[Defender] = { "MOVE", bestTargetMonster.coords.x + bestTargetMonster.vx, bestTargetMonster.coords.y + bestTargetMonster.vy };
            // }
            // если и фармить некого - идем на противоположную позицию фарма от фермера
            // либо на 1-ю дефолтную позицию для фарма
            else {
                if (defenderCurrentTargetPos == -1) {
                    defenderCurrentTargetPos = 0;
                }
                else {
                    defenderCurrentTargetPos = 1;
                }

                if ((our[Defender].coords.x == farmCoordVec[0].x)
                    && (our[Defender].coords.y == farmCoordVec[0].y)) {
                    defenderCurrentTargetPos = 1;
                }
                else if ((our[Defender].coords.x == farmCoordVec[1].x)
                    && (our[Defender].coords.y == farmCoordVec[1].y)) {
                    defenderCurrentTargetPos = 0;
                }
                actions[Defender] = { "MOVE", farmCoordVec[defenderCurrentTargetPos].x, farmCoordVec[defenderCurrentTargetPos].y };
            }
        }

        }

        return actions;
    }

    Coord getMinimalAttackCoord() {
        return {
            abs(enemyBaseCoords.x - static_cast<int>(ceil((300 + cHeroVisibleAreaRadius) * cos(45. * M_PI / 180.)))),
            abs(enemyBaseCoords.y - static_cast<int>(ceil((300 + cHeroVisibleAreaRadius) * sin(45. * M_PI / 180.))))
        };
    }

    vector<Coord> getFarmDefaultCoords() {
        static const double farmRadiusCoef = 1.4;
        return {
            Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * farmRadiusCoef * cos(20. * M_PI / 180.)))),
                    abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * farmRadiusCoef * sin(20. * M_PI / 180.)))) },
            Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * farmRadiusCoef * cos(70. * M_PI / 180.)))),
                    abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * farmRadiusCoef * sin(70. * M_PI / 180.)))) }
        };
    }

    vector<Coord> getAttackDefaultCoords() {
        static const double attackRadiusCoef = 1.3;
        /*
        cerr << "Enemy base coords: " << enemyBaseCoords.x << ", " << enemyBaseCoords.y << endl;
        cerr << "\t\tcoords1 dx: " << static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * cos(20. * M_PI / 180.))) << ", "
                         << "dy: " << static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * sin(20. * M_PI / 180.))) << endl;
        cerr << "\t\tcoords2 dx: " << static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * cos(70. * M_PI / 180.))) << ", "
                         << "dy: " << static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * sin(70. * M_PI / 180.))) << endl;
        */
        return {
            Coord{  abs(enemyBaseCoords.x - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * cos(20. * M_PI / 180.)))),
                    abs(enemyBaseCoords.y - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * sin(20. * M_PI / 180.)))) },
            Coord{  abs(enemyBaseCoords.x - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * cos(70. * M_PI / 180.)))),
                    abs(enemyBaseCoords.y - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * sin(70. * M_PI / 180.)))) }
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
        for (const Action& action : entities.prepairActions()) {
            cout << action.action << " " << action.val1;
            if (action.val2 != -1 || action.action == "MOVE") cout << " " << action.val2;
            if (action.val3 != -1) cout << " " << action.val3;
            cout << endl;
        }
        turnCnt++;
        cerr << "time per turn: " << std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count() << " ms\n";
    }
}