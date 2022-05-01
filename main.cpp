#define _USE_MATH_DEFINES
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <list>
#include <set>
#include <string>
#include <vector>
#include <optional>

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
const double cBaseVisibilityRadius{ 6000 };
const Coord cMaxCoord{ 17630, 9000 };
const Coord cInvalidCoord{ -17630, -9000 };
const double cMaxDist = dist({ 0, 0 }, cMaxCoord);
const int cMonsterSpeed = 400;
const int cUnitSpeed = 800;
const int cUnitDamage = 2;
int turnCnt = 0;
const int cFarmingPeriod = 105; // количество ходов в начале игры, в течение которого фармим
const double cBaseAttackRadius = 300.;
const int cMaxTurnIdx = 219;
const int cHardGameBeginTurn = 105;
const int cWindSpellRadius = 1280;
const int cWindDistance = 2200;
const int cHalfWindDistance = cWindDistance / 2;
const int cMaxSimulationDepth = 45;
const double cHeroVisibleAreaRadius = 2200.;
const double cDangerousDistanceThreshold = 12000.; // порог криволинейного расстояния до нашей базы, после которого монстр считается ОПАСНЫМ для нас
const double cDangerousDistanceEnemyThreshold = 7500.;
const double cImportanceStep = 2500.; // важность монстра, уменьшающаяся с каждым ходом на величину шага
const double cReachBaseAttackRadius = 200.;
const double cCoordsInaccuracy = 2.;
const double cUnitAttackRadius = 800.;
const double cControlSpelDistance = 2200.;

const double cMaxDangerousMonsterFarmDistance = 6000.;
const double cMaxFarmMonsterFarmDistance = 4000.;
const int cFatalityManaThreshold = 120;
const int cFatalityMonsterMinHealthThreshold = 8;

struct BaseStats {
    int health, mana;
};
enum EntityTypes {
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
    Coord simulatedCoords;
    Coord baseRadiusReachCoord{ cInvalidCoord }; // координата, в которой монстр достигает радиуса базы. если эта координата за пределами поля - монстр не интересует
    bool goingWithinWindRadiusInFatalCoord{ false }; // пройдет ли монстр в радиусе задува
    bool isAlreadyAttackedByOurUnit{ false }; // атакует ли уже наш юнит этого монстра
    double nearestDistanceToOurBase{ 0. };
    bool isAlreadyControlled{ false };

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
int getIdxOfNearestNotAttackedUnits(const Coord& targetCoords, const vector<Entity>& potentialEntities) {
    double bestDistBetweenUnitAndTarger = cMaxDist, curDist;
    int bestUnitId = -1;
    for (const Entity& potentUnit : potentialEntities) {
        curDist = dist(potentUnit.coords, targetCoords);
        if (curDist < bestDistBetweenUnitAndTarger
            && !potentUnit.isAlreadyAttackedByOurUnit) {
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
    for (auto iter = vec.begin(); iter != vec.end(); iter++) {
        if (iter->id == id) {
            vec.erase(iter);
            return;
        }
    }
}
int getWindCastAllowRadius() {
    return 5000; // +1000 * bases.ourBase.mana / 40;
}
bool isEntityInCoord(const Entity& entity, const Coord& coord, int availableRaius = 0) {
    return dist(entity.coords, coord) < (2 + availableRaius);
}
double getEntityAlphaToCoords(const Entity& entity, const Coord& coord) {
    double dx = abs(coord.x - entity.coords.x),
        dy = abs(coord.y - entity.coords.y),
        _dist = dist(coord, entity.coords);
    return 0;
}
double getMinDistanceToEntity(const Entity& target, const vector<Entity>& someEnteties) {
    double minDist = cMaxDist;
    for (const Entity& someEnt : someEnteties) {
        double curDist = dist(someEnt.coords, target.coords);
        if (curDist < minDist) {
            minDist = curDist;
        }
    }
    return minDist;
}
Entity* getEntityById(vector<Entity>& someEnteties, int targetId) {
    for (Entity& ent : someEnteties) {
        if (ent.id == targetId) {
            return &ent;
        }
    }
    return nullptr;
}
vector<Entity> getEnemiesAttackedEntity(const vector<Entity>& enemiesVec, const Entity& entity, const Coord& enemyBaseAttackCoord) {
    vector<Entity> enemiesAttackedEntity;
    double entityDistToBase = dist(entity.coords, enemyBaseAttackCoord);
    int entityStepsToBase = ceil(entityDistToBase / cMonsterSpeed);

    for (const Entity& enemy : enemiesVec) {
        double enemyUnitStepsToBase = floor(dist(enemy.coords, enemyBaseAttackCoord) / cUnitSpeed);
        if (dist(enemy.coords, entity.coords) < cUnitAttackRadius * 2
            || enemyUnitStepsToBase <= entityStepsToBase) {
            enemiesAttackedEntity.push_back(enemy);
        }
    }
    return enemiesAttackedEntity;
}
void removeEntityWithId(int entId, vector<Entity>& vec) {
    for (int i = 0; i < vec.size(); i++) {
        if (vec[i].id == entId) {
            vec.erase(vec.begin() + i);
            return;
        }
    }
}
Coord getEntityBaseTargetCoord(const Entity& ent, const Coord& baseCoord) {
    double distToBase = dist(ent.coords, baseCoord);
    double entDX = abs(baseCoord.x - ent.coords.x),
           entDY = abs(baseCoord.y - ent.coords.y);
    double targetBaseCoordDX = entDX * cBaseAttackRadius / distToBase,
           targetBaseCoordDY = entDY * cBaseAttackRadius / distToBase;
    return {
        static_cast<int>(ceil(abs(targetBaseCoordDX - baseCoord.x))), 
        static_cast<int>(ceil(abs(targetBaseCoordDY - baseCoord.y)))
    };
}
bool needMonsterControl(const Entity& monster, Coord enemyBase) {
    return abs(enemyBase.y - monster.coords.y) < 3400;
}

vector<int> getMonstersIdxsWithinCoord(const Coord& targetCoords, vector<Entity>& monstersVec) {
    vector<int> nearestMonsters;
    for (Entity& ent : monstersVec) {
        if (dist(ent.coords, targetCoords) < cHeroVisibleAreaRadius) {
            nearestMonsters.push_back(ent.id);
        }
    }
    return nearestMonsters;
}
Entity& getMonsterWithMaxImportnace(vector<int> idxs, vector<Entity>& monstersVec) {
    int maxImportance = -1;
    int resultIdx;
    for (int i = 0; i < monstersVec.size(); i++) {
        if (monstersVec[i].importance > maxImportance) {
            maxImportance = monstersVec[i].importance;
            resultIdx = i;
        }
    }
    return monstersVec[resultIdx];
}
Coord getBestFatalityWindCoord(Coord fatalityPos, Entity& monster, Coord enemyBaseCoord) {
    Coord monsterTargetCoord = getEntityBaseTargetCoord(monster, enemyBaseCoord);
    int dx = fatalityPos.x - monster.coords.x,
        dy = fatalityPos.y - monster.coords.y;
    return { monsterTargetCoord.x + dx, monsterTargetCoord.y + dy };
}
Entity* getBestMonsterToPush(vector<Entity>& monsters, Coord fatalityPos) {
    Entity* bestMonster = nullptr;

    for (Entity& monster : monsters) {
        
        if (dist(monster.coords, fatalityPos) < cWindSpellRadius
            && monster.health > cFatalityMonsterMinHealthThreshold
            && !monster.shieldLife)
        {
            return &monster;
        }
    }

    return bestMonster;
}


struct Entities {
    vector<Entity> our, enemies, monsters;
    Coord ourBaseCoords, enemyBaseCoords, enemyBaseAttackCoords;
    int entitiesNb;

    bool FatalityFarmFlag{ true };
    vector<Coord> FatalityFarmCoords;
    Coord FatalityPos, FatalityMonsterPos;
    int controlYCoordThreshold;
    
    void readBaseCoords() {
        cin >> ourBaseCoords.x >> ourBaseCoords.y; cin.ignore();
        enemyBaseCoords = { cMaxCoord.x - ourBaseCoords.x, cMaxCoord.y - ourBaseCoords.y };

        FatalityFarmCoords = {
            Coord{ abs(ourBaseCoords.x - 3204), abs(ourBaseCoords.y - 7350) },
            Coord{ abs(ourBaseCoords.x - 7127), abs(ourBaseCoords.y - 7100) },
            Coord{ abs(ourBaseCoords.x - 7616), abs(ourBaseCoords.y - 923) },
        };

        FatalityMonsterPos = { abs(enemyBaseCoords.x - 4830), abs(enemyBaseCoords.y - 200) };
        FatalityPos = { abs(enemyBaseCoords.x - 6600), abs(enemyBaseCoords.y - 1200) };
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

    // ------------------------- FATALITY -------------------------
    void getFatalityFarming(Actions& actions, vector<Entity>& dangerousMonsters, vector<Entity>& farmingMonsters) {
        cerr << "---getFatalityFarming---" << endl;

        // отдельно обрабатываем опасных монстров
        // выбираем наших ближайших юнитов
        set<int> ourUsedUnits;
        vector<Entity> ourAvailableUnits(our.begin(), our.end());
        for (Entity& monster : dangerousMonsters) {
            int ourNearestUnitId = getIdxOfNearestUnits(monster.coords, ourAvailableUnits);
            actions[convertToArrayIdx(ourNearestUnitId)] = { "MOVE", monster.coords.x + monster.vx, monster.coords.y + monster.vy };

            cerr << "\t Our unit " << our.at(convertToArrayIdx(ourNearestUnitId)).id << " will go to FARM DANGER nearest monster " << monster.id << endl;
            monster.isAlreadyAttackedByOurUnit = true;

            eraseFromVecUnitId(ourNearestUnitId, ourAvailableUnits);
            ourUsedUnits.insert(convertToArrayIdx(ourNearestUnitId));
        }


        for (int unitIdx = 0; unitIdx < 3; unitIdx++) {

            if (ourUsedUnits.count(unitIdx)) continue;

            Entity& ourUnit = our.at(unitIdx);
            Coord& unitFarmingCoord = FatalityFarmCoords.at(unitIdx);

            int nearestDangerMonsterId = getIdxOfNearestNotAttackedUnits(ourUnit.coords, dangerousMonsters);
            int nearestFarmMonsterId = getIdxOfNearestNotAttackedUnits(ourUnit.coords, farmingMonsters);

            // cerr << "\t For unit " << ourUnit.id << " nearestDangerMonsterId: " << nearestDangerMonsterId << endl;
            // cerr << "\t For unit " << ourUnit.id << " nearestFarmMonsterId: " << nearestFarmMonsterId << endl;

            // отдельно учитываем сразу два вхождения, выбираем ближайшего монстра
            if (nearestDangerMonsterId != -1 && nearestFarmMonsterId != -1)
            {
                Entity* nearestDangerMonster = getEntityById(dangerousMonsters, nearestDangerMonsterId);
                Entity* nearestFarmMonster = getEntityById(farmingMonsters, nearestFarmMonsterId);

                double distToDanger = dist(ourUnit.coords, nearestDangerMonster->coords),
                    distToFarm = dist(ourUnit.coords, nearestFarmMonster->coords),
                    dangerDistToFarmCoord = dist(unitFarmingCoord, nearestDangerMonster->coords),
                    farmDistToFarmCoord = dist(unitFarmingCoord, nearestFarmMonster->coords);

                cerr << "\t Our unit " << ourUnit.id << " will go to FARM DANGER nearest monster " << nearestDangerMonster->id << endl;
                actions[unitIdx] = { "MOVE", nearestDangerMonster->coords.x + nearestDangerMonster->vx, nearestDangerMonster->coords.y + nearestDangerMonster->vy };
                nearestDangerMonster->isAlreadyAttackedByOurUnit = true;

            }
            else if (Entity* nearestDangerMonster = getEntityById(dangerousMonsters, nearestDangerMonsterId);
                nearestDangerMonster)
                // && dist(nearestDangerMonster->coords, unitFarmingCoord) < cMaxDangerousMonsterFarmDistance)
            {
                cerr << "\t Our unit " << ourUnit.id << " will go to FARM DANGER monster " << nearestDangerMonster->id << endl;
                actions[unitIdx] = { "MOVE", nearestDangerMonster->coords.x + nearestDangerMonster->vx, nearestDangerMonster->coords.y + nearestDangerMonster->vy };
                nearestDangerMonster->isAlreadyAttackedByOurUnit = true;
            }
            else if (Entity* nearestFarmMonster = getEntityById(farmingMonsters, nearestFarmMonsterId);
                nearestFarmMonster
                && dist(nearestFarmMonster->coords, unitFarmingCoord) < cMaxFarmMonsterFarmDistance)
            {
                cerr << "\t Our unit " << ourUnit.id << " will go to FARM NOT DANGER monster " << nearestFarmMonster->id << endl;
                actions[unitIdx] = { "MOVE", nearestFarmMonster->coords.x + nearestFarmMonster->vx, nearestFarmMonster->coords.y + nearestFarmMonster->vy };
                nearestFarmMonster->isAlreadyAttackedByOurUnit = true;
            }
            else {
                // иначе идем в точку фарма
                actions[unitIdx] = { "MOVE", unitFarmingCoord.x, unitFarmingCoord.y };
            }
        }
    }
    void getFatalityPushing(Actions& actions, vector<Entity>& dangerousMonsters, vector<Entity>& monsters) {
        cerr << "---getFatalityPushing---" << endl;

        bool isAllUnitsInfatalityPos = true;

        for (int unitIdx = 0; unitIdx < 3; unitIdx++) {
            isAllUnitsInfatalityPos &= isEntityInCoord(our.at(unitIdx), FatalityPos);
        }

        if (isAllUnitsInfatalityPos) {

            cerr << "\t All units in FatalityPos, find monster to winding" << endl;

            // если все в сборе, то ищем кого запушить
            // Entity* monsterToPush = getEntityById(monsters, getIdxOfNearestUnits(FatalityPos, monsters));
            Entity* monsterToPush = getBestMonsterToPush(monsters, FatalityPos);

            if (monsterToPush)
            {
            // если монстр в области видимости существует
                Coord monsterTargetCoord = getEntityBaseTargetCoord(*monsterToPush, enemyBaseCoords);
                double distToEnemyBase = dist(monsterToPush->coords, monsterTargetCoord);

                // если монстр гарантировано запушится на базу врага, то пушим
                if ( // distToEnemyBase < 3 * (cWindDistance + cCoordsInaccuracy) &&
                   dist(monsterToPush->coords, FatalityPos) < cWindSpellRadius) // если уже сейчас достаем до монстра, то пушим сразу
                {
                    Coord bestCoordToWind = getBestFatalityWindCoord(FatalityPos, *monsterToPush, enemyBaseCoords);
                    cerr << "\t All units in FatalityPos, try WIND monster " << monsterToPush->id << " to coord " << bestCoordToWind.x << ", " << bestCoordToWind.y << endl;
                    for (int unitIdx = 0; unitIdx < 3; unitIdx++) {
                        bases.ourBase.mana -= 10;
                        actions[unitIdx] = { "SPELL WIND", bestCoordToWind.x, bestCoordToWind.y };
                    }
                }
                // если этот монстр пока не рядом, но пройдет рядом
                else if (monsterToPush->goingWithinWindRadiusInFatalCoord) {
                    cerr << "\t monster " << monsterToPush->id << " will going Within radius in fatal coord" << endl;
                    for (int unitIdx = 0; unitIdx < 3; unitIdx++) {
                        actions[unitIdx] = { "WAIT" };
                    }
                }
                // если нет монстра для задува
                // и нет монстра который пройдет рядом с задувом
                // но есть монстры которых можно законтрить
                else if (bases.ourBase.mana - bases.enemyBase.health * 30 >= 10) {

                    // за один ход контролим только одного монстра
                    int controoleMonstersNb = 0;
                    for (Entity& monster : monsters) {
                        if (!monster.goingWithinWindRadiusInFatalCoord
                            && monster.health > cFatalityMonsterMinHealthThreshold // и у монстра достаточно хп
                            && !monster.shieldLife
                            ) { 
                            // если монстр не пройдет через FatalityPos
                            // то сами его туда направим
                            monster.goingWithinWindRadiusInFatalCoord = true;
                            bases.ourBase.mana -= 10;
                            cerr << "Controll monster " << monster.id << " because 3" << endl;
                            actions[controoleMonstersNb++] = { "SPELL CONTROL", monster.id, FatalityMonsterPos.x, FatalityMonsterPos.y };
                            break;
                        }
                    }
                    // остальные юниты пока бездействуют
                    for (int unitIdx = controoleMonstersNb; unitIdx < 3; unitIdx++) {
                        actions[unitIdx] = { "WAIT" };
                    }
                }
                // если задувать некого и контрлоить некого - ждем
                else {
                    for (int unitIdx = 0; unitIdx < 3; unitIdx++) {
                        actions[unitIdx] = { "WAIT" };
                    }
                }
            }
            else if (bases.ourBase.mana - bases.enemyBase.health * 30 >= 10) {
                // если нет монстра для задува, но есть монстры которых можно законтрить
                int controoleMonstersNb = 0;
                for (Entity& monster : monsters) {
                    if (!monster.goingWithinWindRadiusInFatalCoord
                        && monster.health > cFatalityMonsterMinHealthThreshold // и у монстра достаточно хп
                        && !monster.shieldLife
                        ) { 
                        // если монстр не пройдет через FatalityPos
                        // то сами его туда направим
                        monster.goingWithinWindRadiusInFatalCoord = true;
                        bases.ourBase.mana -= 10;
                        cerr << "Controll monster " << monster.id << " because 4" << endl;
                        actions[controoleMonstersNb++] = { "SPELL CONTROL", monster.id, FatalityMonsterPos.x, FatalityMonsterPos.y };
                    }
                    if (controoleMonstersNb == 3) {
                        break;
                    }
                }

                for (int unitIdx = controoleMonstersNb; unitIdx < 3; unitIdx++) {
                    actions[unitIdx] = { "WAIT" };
                }
            }
            else {
                for (int unitIdx = 0; unitIdx < 3; unitIdx++) {
                    actions[unitIdx] = { "WAIT" };
                }
            }
        }
        else {
            cerr << "\t All units NOT in FatalityPos, going to pos..." << endl;

            for (int unitIdx = 0; unitIdx < 3; unitIdx++) {
                bool isUnitInFatalityPos = isEntityInCoord(our.at(unitIdx), FatalityPos);
                if (isUnitInFatalityPos) {
                    actions[unitIdx] = { "WAIT" };
                }
                else {

                    // проверяем опасных монстров
                    vector<int> nearestDangerousMonstersIdx = getMonstersIdxsWithinCoord(our.at(unitIdx).coords, dangerousMonsters);
                    cerr << "\t Near unit " << unitIdx << " exists " << nearestDangerousMonstersIdx.size() << " dangerous monsters" << endl;

                    if (nearestDangerousMonstersIdx.size()) {
                        Entity& maxImportanceMonster = getMonsterWithMaxImportnace(nearestDangerousMonstersIdx, dangerousMonsters);
                        if (maxImportanceMonster.health > 2
                            && !maxImportanceMonster.isAlreadyControlled
                            && (bases.ourBase.mana - bases.enemyBase.health * 30 >= 10)
                            && !maxImportanceMonster.shieldLife
                            && maxImportanceMonster.distanceToReachBase < 9000.
                            && dist(maxImportanceMonster.coords, ourBaseCoords) > cBaseRadius
                            )
                        {
                            maxImportanceMonster.isAlreadyControlled = true;
                            maxImportanceMonster.goingWithinWindRadiusInFatalCoord = true;
                            bases.ourBase.mana -= 10;
                            cerr << "Controll monster " << maxImportanceMonster.id << " because it can push OUR BASE" << endl;
                            actions[unitIdx] = { "SPELL CONTROL", maxImportanceMonster.id, FatalityMonsterPos.x, FatalityMonsterPos.y };
                            continue;
                        }
                    }

                    // если по пути встречаем монстра, которого удобно законтролить на будущее
                    vector<Entity> monstersNearCurUnit; // список монстров, которых можно законтролить
                    for (Entity& monster : monsters) {
                        if (dist(monster.coords, ourBaseCoords) > 6200. // достаточно далеко от нашей базы
                            && dist(monster.coords, our.at(unitIdx).coords) < cHeroVisibleAreaRadius // можем этим юнитом законтролить
                            && !monster.isAlreadyControlled // его еще не контроялт другие юниты
                            && !monster.goingWithinWindRadiusInFatalCoord // и он сам не пройдет через координату задува
                            && monster.health > cFatalityMonsterMinHealthThreshold // и у монстра достаточно хп
                            && !monster.shieldLife
                            && needMonsterControl(monster, enemyBaseCoords)
                            )
                        {
                            monstersNearCurUnit.push_back(monster);
                        }
                    }

                    // если нашлись такие монстры
                    // то выберем того, который ближайший к точке задува
                    if (!monstersNearCurUnit.empty()
                        && (bases.ourBase.mana - bases.enemyBase.health * 30 >= 10))
                    {
                        Entity* monsterToControl = getEntityById(monsters, getIdxOfNearestUnits(FatalityPos, monstersNearCurUnit));
                        monsterToControl->isAlreadyControlled = true;
                        monsterToControl->goingWithinWindRadiusInFatalCoord = true;
                        bases.ourBase.mana -= 10;
                        cerr << "Controll monster " << monsterToControl->id << " because 2" << endl;
                        actions[unitIdx] = { "SPELL CONTROL", monsterToControl->id, FatalityMonsterPos.x, FatalityMonsterPos.y };
                    }
                    else {
                        actions[unitIdx] = { "MOVE", FatalityPos.x, FatalityPos.y };
                    }

                }
            }
        }
    }


    // ------------------------- PREPAIR ACTIONS -------------------------

    Actions prepairActions() {

        vector<Entity> ourDangerousMonsters, ourMonstersToFarming;
        set<int> ourDangerousAccountedMonsters;

        for (Entity& monster : monsters) {
            monster.simulatedCoords = monster.coords;
        }

        // симуляция движений видимых сущностей
        double curDistToOurBase;
        for (int simulationDepth = 0; simulationDepth < cMaxSimulationDepth; simulationDepth++) {
            for (Entity& monster : monsters) {

                // симулируем координаты монстра и учитываем его дистанции
                monster.simulatedCoords.x += monster.vx;
                monster.simulatedCoords.y += monster.vy;

                monster.goingWithinWindRadiusInFatalCoord |= (dist(monster.simulatedCoords, FatalityMonsterPos) < 1280);

                // дистанция до баз на i-м шаге симуляция
                curDistToOurBase = dist(monster.simulatedCoords, ourBaseCoords);

                monster.nearestDistanceToOurBase = min(monster.nearestDistanceToOurBase, curDistToOurBase);

                // меняеем вектор скорости, когда монстр заходит в радиус базы
                if (curDistToOurBase < cBaseRadius)
                {
                    // запоминаем координату, на которой монстр заходит в радиус
                    if (monster.baseRadiusReachCoord.x == cInvalidCoord.x) {
                        monster.baseRadiusReachCoord = monster.simulatedCoords;
                        // cerr << "Monster " << monster.id << " will reach our base at coords (" << monster.baseRadiusReachCoord.x << " ," << monster.baseRadiusReachCoord.y << ")" << endl;
                    }
                    int dBaseX = ourBaseCoords.x - monster.simulatedCoords.x, // важно чтобы значения могли быть отрицательным
                        dBaseY = ourBaseCoords.y - monster.simulatedCoords.y; // для правильных значений компонент скорости
                    monster.vx = ceil(cMonsterSpeed * dBaseX / curDistToOurBase);
                    monster.vy = ceil(cMonsterSpeed * dBaseY / curDistToOurBase);
                }

                // учет положений и состояний пауков
                // если монстр еще не учтен для нас
                if (!ourDangerousAccountedMonsters.count(monster.id)) {
                    monster.distanceToReachBase += cMonsterSpeed;
                    monster.stepsToBase++;

                    // ОПАСНЫЕ ДЛЯ НАШЕЙ БАЗЫ
                    // если по результатам симуляции монстр дошел до базы - считаем его опасным
                    if (curDistToOurBase < cBaseRadius + 1500.
                        && monster.baseRadiusReachCoord.x >= 0 && monster.baseRadiusReachCoord.x <= cMaxCoord.x  // валидность координат
                        && monster.baseRadiusReachCoord.y >= 0 && monster.baseRadiusReachCoord.y <= cMaxCoord.y) // валидность координат
                    {
                        monster.importance = (cMaxSimulationDepth - simulationDepth) * cImportanceStep;

                        cerr << "Monster " << monster.id << " is DANGER for our base, importance: " << monster.importance << endl;

                        ourDangerousAccountedMonsters.insert(monster.id);
                        ourDangerousMonsters.push_back(monster);
                    }
                }
            }
        }

        // учет монстров, которых можно пофармить
        for (Entity& monster : monsters) {
            double curDistToOurBase = dist(monster.coords, ourBaseCoords);

            if (!ourDangerousAccountedMonsters.count(monster.id)) // если монстр не опасный для нас
            {
                // важность монстра считается по его ТЕКУЩЕМУ положению
                monster.importance = (cMaxDist - curDistToOurBase) * 2500. / cMaxDist;
                ourMonstersToFarming.push_back(monster);
            }
        }

        // сортировка по важности
        sort(ourDangerousMonsters.begin(), ourDangerousMonsters.end(), entityCompare);
        sort(ourMonstersToFarming.begin(), ourMonstersToFarming.end(), entityCompare);

        // формируем ходы
        Actions actions; actions.resize(3);

        FatalityFarmFlag &= (bases.ourBase.mana < cFatalityManaThreshold);

        if (FatalityFarmFlag) {
        // фармим монстров в радиусах позиций
            getFatalityFarming(actions, ourDangerousMonsters, ourMonstersToFarming);
        }
        else {
        // идем в позицию пуша
            getFatalityPushing(actions, ourDangerousMonsters, monsters);
        }

        return actions;
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