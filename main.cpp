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
enum EntityType {
    Defender,
    Attacker,
    Farmer
};
double dist(const Coord& c1, const Coord& c2) {
    return sqrt(pow(abs(c1.x - c2.x), 2) + pow(abs(c1.y - c2.y), 2));
}
const double cBaseRadius{ 5000 };
const Coord cMaxCoord{ 17630, 9000 };
const Coord cInvalidCoord{ -17630, -9000 };
const double cMaxDist = dist({ 0, 0 }, cMaxCoord);
const int cMonsterSpeed = 400;
const int cUnitSpeed = 800;
const int cUnitDamage = 2;
int turnCnt = 0;
const int cFarmingPeriod = 55; // количество ходов в начале игры, в течение которого фармим
const double cBaseAttackRadius = 300.;
const int cMaxTurnIdx = 219;
const int cWindSpellRadius = 1280;
const int cWindDistance = 2200;
const int cHalfWindDistance = cWindDistance / 2;
const int cMaxSimulationDepth = 35;
const double cHeroVisibleAreaRadius = 2200.;
const double cDistanceThresholdWhenEnemyReachBase = 500.; // расстояние, на котором считается, что монстр достиг базы
const double cDangerousDistanceThreshold = 9000.; // порог криволинейного расстояния до нашей базы, после которого монстр считается ОПАСНЫМ для нас
const double cDangerousDistanceEnemyThreshold = 7500.;
const double cImportanceStep = 2500.; // важность монстра, уменьшающаяся с каждым ходом на величину шага
const double cCoordsInaccuracy = 2.;
const double cUnitAttackRadius = 800.;
const double cControlSpelDistance = 2200.;

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
    int neededUnitsNb{ 0 }; // МИНИМАЛЬНОЕ количество юнитов, требующееся для убийства монстра
    Coord simulatedCoords;
    double minDistanceToBase{ cMaxDist }; // минимальное расстояние до базы, которого достигнет монстр за все симуляции (положительное для нашей базы, отрицательное - для вражеской)
    Coord baseRadiusReachCoord{ cInvalidCoord }; // координата, в которой монстр достигает радиуса базы. если эта координата за пределами поля - монстр не интересует

    // bool needWind{ false };

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
    return 5000; // +1000 * bases.ourBase.mana / 40;
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


struct Entities {
    vector<Entity> our, enemies, monsters;
    Coord ourBaseCoords, enemyBaseCoords, enemyBaseAttackCoords;
    int entitiesNb;

    int PushMonsterId; // id того монстра, которого пробуем запушить

    int totalCurTreatment{ 0 }; // текущий уровень угрозы для базы

    Coord minimalAttackCoord;
    vector<Coord> attackCoordsVec, attackerFarmCoordVec;
    vector<vector<Coord>> waitCoordsVec2;

    Coord bottomWaitCoord, topWaitCoord;
    int bottomWaitCoordRotting{ 0 },
        topWaitCoordRotting{ 0 };

    int farmerCurrentTargetPos = -1,
        attackerCurrentTargetPos = -1,
        defenderCurrentTargetPos = -1,
        attackerFarmingCurPos = -1;

    void readBaseCoords() {
        cin >> ourBaseCoords.x >> ourBaseCoords.y; cin.ignore();
        enemyBaseCoords = { cMaxCoord.x - ourBaseCoords.x, cMaxCoord.y - ourBaseCoords.y };
        minimalAttackCoord = getMinimalAttackCoord();
        attackCoordsVec = getAttackDefaultCoords();
        attackerFarmCoordVec = getAttackerFarmCoords();
        updateWaitCoords();
        waitCoordsVec2 = getWaitCoords();
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


    // ------------------------- ATTACKER -------------------------
    optional<Action> tryAttackerWindMonster(const vector<Entity>& enemiesVec, const Coord& entityBaseTargetCoord, const Entity& entToWind) {
        cerr << "---tryAttackerWindMonster---" << endl;

        if (double distToAttackMonster = dist(entToWind.coords, our[Attacker].coords); distToAttackMonster > cWindSpellRadius) {
            cerr << "\tAttacker can not wind monster, because monster " << entToWind.id << " is far away on dist " << distToAttackMonster << endl;
            return nullopt;
        }
        if (bases.ourBase.mana < 10) {
            cerr << "\tAttacker can not wind monster, because mana is too low: " << bases.ourBase.mana << endl;
            return nullopt;
        }
        if (bases.ourBase.mana < 10) {
            cerr << "\tAttacker can not wind monster, because exist nearest defender: " << enemiesVec.size() << " nb " << endl;
            return nullopt;
        }

        // начинаеш пуш
        cerr << "\tAttacker start push monster " << entToWind.id << endl;
        PushMonsterId = entToWind.id;
        bases.ourBase.mana -= 10;
        attackerCurrentTargetPos = -1;
        return Action{ "SPELL WIND", entityBaseTargetCoord.x, entityBaseTargetCoord.y };
    }
    optional<Action> tryAttackerControlEnemyDefender(const vector<Entity>& enemiesVec, const Entity& entToProtect) {
        cerr << "---tryAttackerControlEnemyDefender---" << endl;
        // если рядом с ним всего 1 вражеский юнит
        // И хп пушащего монстра больше 2
        // ТО пытаемся сбить вражеского юнита

        if (enemiesVec.size() != 1) {
            cerr << "\tAttacker can not control enemy defender, because enemiesVec.size() == " << enemiesVec.size() << endl;
            return nullopt;
        }
        if (bases.ourBase.mana < 10) {
            cerr << "\tAttacker can not control enemy defender, because mana is too low: " << bases.ourBase.mana << endl;
            return nullopt;
        }
        if (entToProtect.health <= 2) {
            cerr << "\tAttacker can not control enemy defender, because entity " << entToProtect.id << " has too low health" << endl;
            return nullopt;
        }
        if (enemiesVec.at(0).shieldLife) {
            cerr << "\tAttacker can not control enemy defender, because entity " << entToProtect.id << " has shield" << endl;
            return nullopt;
        }
        if (double distEnemyToBase = dist(enemiesVec.at(0).coords, enemyBaseCoords); distEnemyToBase > 5000) {
            cerr << "\tAttacker can not control enemy defender, because defender is too far away from their base: " << distEnemyToBase << endl;
            return nullopt;
        }
        if (double distEnemyToMonster = dist(enemiesVec.at(0).coords, entToProtect.coords); distEnemyToMonster > cWindSpellRadius + cUnitSpeed) {
            cerr << "\tAttacker not need control enemy defender, because defender is too far away from pushed monstera: " << distEnemyToMonster << endl;
            return nullopt;
        }

        bases.ourBase.mana -= 10;
        cerr << "\tAttacker can CONTROL enemy defender " << enemiesVec.at(0).id << " to protect monster " << entToProtect.id << endl;
        return Action{ "SPELL CONTROL", enemiesVec.at(0).id, ourBaseCoords.x, ourBaseCoords.y };
    }
    void tryAttackerCheckMonsterSurvivability(const vector<Entity>& enemiesVec, Entity& entToProtect, vector<Entity>& enemyDangerousMonsters) {
        cerr << "---tryAttackerCheckMonsterSurvivability---" << endl;
        // если вражеских юнитов больше 1, оцениваем живучесть монстра и по необходимости забываем про него
        if (enemiesVec.size() > 1) {
            entToProtect.health -= enemiesVec.size() * 2;

            if (entToProtect.health < 1) {
                cerr << "monster " << entToProtect.id << " will be killed by defenders" << endl;
                removeEntityWithId(PushMonsterId, enemyDangerousMonsters);
                PushMonsterId = 0;
            }
        }
    }
    optional<Action> TryAttackerHelpToPushingMonster(Entity* pushedEntityPtr, vector<Entity>& enemyDangerousMonsters) {
        cerr << "---TryAttackerHelpToPushingMonster---" << endl;

        cerr << "Exist pushing monster: " << pushedEntityPtr->id << endl;
        // точка в радиусе 300 от базы врага, в которую должен попасть монстр
        const Coord entityBaseTargetCoord = getEntityBaseTargetCoord(*pushedEntityPtr, enemyBaseCoords);

        // вражеские деферы
        auto enemiesAttackedEntityVec = getEnemiesAttackedEntity(enemies, *pushedEntityPtr, entityBaseTargetCoord);
        cerr << "Visible defender-units nb for attacker: " << enemiesAttackedEntityVec.size() << endl;

        // если вражеский юнит 1, то контрим его
        if (auto act = tryAttackerControlEnemyDefender(enemiesAttackedEntityVec, *pushedEntityPtr); act.has_value()) return act;

        // пробуем задуть если деферы не помешают
        if (auto act = tryAttackerWindMonster(enemiesAttackedEntityVec, entityBaseTargetCoord, *pushedEntityPtr); act.has_value()) return act;

        // проверяем живучесть монстра под вражескими деферами и если надо - забываем про него
        tryAttackerCheckMonsterSurvivability(enemiesAttackedEntityVec, *pushedEntityPtr, enemyDangerousMonsters);

        // пробуем защитить пушащего монстра с хорошим кол-вом хп
        vector<Entity> pushingMonsters = { *pushedEntityPtr };
        if (auto act = tryAttackerShieldOtherPushingMonsters(pushingMonsters); act.has_value()) return act;

        return nullopt;
    }
    Action tryAttackerGoToNextPos() {

        // надо выбрать куда идти
        Coord attackerTargerCoord;
        bool isAttackerInBestPos = isEntityInCoord(our[Attacker], minimalAttackCoord);
        bool isAttackerInWatchPos0 = isEntityInCoord(our[Attacker], attackCoordsVec[0]);
        bool isAttackerInWatchPos1 = isEntityInCoord(our[Attacker], attackCoordsVec[1]);

        if (PushMonsterId && !isAttackerInBestPos) {
            // если был ранее задуваемый - идем вглубь
            // как только доходим до глубины - идем на обзорные позиции
            attackerCurrentTargetPos = -1;
            attackerTargerCoord = minimalAttackCoord;
            cerr << "Attacker have already pushed monster with id " << PushMonsterId << ", try to find him and help" << endl;
        }
        else {
            // если пришли на самую глубокую позицию и монстров не видим
            // сбрасываем счетчики
            if (isAttackerInBestPos) {
                PushMonsterId = 0;
            }

            // выбираем таргет-позицию куда дальше идти
            if (attackerCurrentTargetPos == -1) {
                attackerCurrentTargetPos = 0;
            }
            if (isAttackerInWatchPos0) {
                attackerCurrentTargetPos = 1;
            }
            else if (isAttackerInWatchPos1) {
                attackerCurrentTargetPos = 0;
            }
            cerr << "Attacker have not pushed monster, trying to return in base position " << attackerCurrentTargetPos << endl;
            attackerTargerCoord = attackCoordsVec[attackerCurrentTargetPos];
        }
        return { "MOVE", attackerTargerCoord.x, attackerTargerCoord.y };
    }
    optional<Action> tryAttackerShieldOtherPushingMonsters(vector<Entity>& enemyDangerousMonsters) {
        cerr << "---tryAttackerShieldOtherPushingMonsters---" << endl;

        if (bases.ourBase.mana < 10) {
            cerr << "Not enough mana to protect any monsters" << endl;
            return nullopt;
        }

        // пробуем навесить щиток на монстра, у которого хп позволяют дойти до вражеской базы живым
        // выбираем из отсортированных по важности монстров
        for (Entity& ent : enemyDangerousMonsters) {

            // если у монстра уже есть щиток - не процессим его
            if (ent.shieldLife) {
                cerr << "Cant protect monster " << ent.id << " because it has shiled" << endl;
                continue;
            }

            // надо посчитать кол-во шагов, за которое монстр доберется до entityBaseTargetCoord
            Coord monsterTargetEnemyBaseCoord = getEntityBaseTargetCoord(ent, enemyBaseCoords); // точка в радиусе 300, куда метит монстр
            double monsterDistToBase = dist(monsterTargetEnemyBaseCoord, ent.coords);
            int monsterStepsToEnemyBase = ceil(monsterDistToBase / cMonsterSpeed);
            // полагаем что монстра будет атаковать 1 вражеский дефер
            // если монстр переживет под щитком пуь до базы врага
            if (ceil(ent.health / cUnitDamage) > monsterStepsToEnemyBase)
            {
                bases.ourBase.mana -= 10;
                PushMonsterId = 0;
                cerr << "\tAttacker can PROTECT monster " << ent.id << " by SHIELD" << endl;
                return Action{ "SPELL SHIELD", ent.id };
            }
        }
        cerr << "Attacker can not protect any monster by shield" << endl;
        return nullopt;
    }
    optional<Action> tryAttackerStartFarming() {

        if (turnCnt > cFarmingPeriod) {
            return nullopt;
        }

        // найти ближайшего монстра из всех, у которого расстояние до нащей базы > cDangerousDistanceThreshold 
        // (того расстояния, на котором монсстр считается опасным для нашей базы)
        double minDistToMonster = cMaxDist;
        Entity* bestEntToAttack = nullptr;
        for (Entity& ent : monsters) {
            if (dist(ent.coords, ourBaseCoords) > (cDangerousDistanceThreshold)
                && dist(ent.coords, our[Attacker].coords) < minDistToMonster) {
                minDistToMonster = dist(ent.coords, our[Attacker].coords);
                bestEntToAttack = &ent;
            }
        }

        if (bestEntToAttack) return Action{ "MOVE", bestEntToAttack->coords.x + bestEntToAttack->vx, bestEntToAttack->coords.y + bestEntToAttack->vy };

        // ищем куда пойти
        bool isAttackerInFarmPos0 = isEntityInCoord(our[Attacker], attackerFarmCoordVec[0]);
        bool isAttackerInFarmPos1 = isEntityInCoord(our[Attacker], attackerFarmCoordVec[1]);

        // выбираем таргет-позицию куда дальше идти
        if (attackerFarmingCurPos == -1) {
            attackerFarmingCurPos = 0;
        }
        if (isAttackerInFarmPos0) {
            attackerFarmingCurPos = 1;
        }
        else if (isAttackerInFarmPos1) {
            attackerFarmingCurPos = 0;
        }
        cerr << "Attacker going to find monsters to FARM in pos " << attackerFarmingCurPos << endl;
        return Action{ "MOVE", attackerFarmCoordVec[attackerFarmingCurPos].x, attackerFarmCoordVec[attackerFarmingCurPos].y };
    }

    Action getAttackerAction(vector<Entity>& enemyDangerousMonsters) {
        double attackerDistToBase = dist(our[Attacker].coords, enemyBaseCoords);

        if (auto act = tryAttackerStartFarming(); act.has_value()) return act.value();

        // !!! в enemyDangerousMonsters монстры без щитков !!!

        cerr << "enemyDangerousMonsters is empty: " << (enemyDangerousMonsters.empty() ? "true" : "false") << endl;
        cerr << "PushMonsterId: " << PushMonsterId << endl;

        if (enemyDangerousMonsters.empty()
            || PushMonsterId) {
            // если ЛИБО список монстров для атаки пустой
            // ЛИБО был запушенный монстр

            set<int> accountedPushMonsterEnemyBase;

            // если есть пушащий монстр из предыдущих ходов
            if (Entity* pushedEntityPtr = getEntityById(enemyDangerousMonsters, PushMonsterId); pushedEntityPtr) {
                // пробуем помочь монстру
                if (auto act = TryAttackerHelpToPushingMonster(pushedEntityPtr, enemyDangerousMonsters); act.has_value()) return act.value();

                // иначе монстр не интересен
                accountedPushMonsterEnemyBase.insert(pushedEntityPtr->id);
            }

            // если атакер глубоко на территории врага и есть ДРУГОЙ монстр для пуша
            // то переключаемся на него
            if (attackerDistToBase < 4000.
                && !enemyDangerousMonsters.empty()) {
                cerr << "atacker is deep in enemy base, try start push other monster" << endl;

                // если топовый монстр уже проверен - проверяем других опасных монстров
                for (Entity& ent : enemyDangerousMonsters) {
                    if (!accountedPushMonsterEnemyBase.count(ent.id)) {
                        // пробуем помочь монстру
                        if (auto act = TryAttackerHelpToPushingMonster(&ent, enemyDangerousMonsters); act.has_value()) return act.value();

                        // иначе монстр не интересен
                        accountedPushMonsterEnemyBase.insert(ent.id);
                        break;
                    }
                }
            }

            // ищем куда пойти для поиска монстров-пушеров
            return tryAttackerGoToNextPos();
        }
        else {
            // если список с пауками не пустой
            // И нет запушенного монстра
            // ТО проходимся по монстрам в порядке приоритета

            for (Entity& dangerousEnemyMonster : enemyDangerousMonsters) {
                cerr << "Exist pushing monster: " << dangerousEnemyMonster.id << endl;
                // точка в радиусе 300 от базы врага, в которую должен попасть монстр
                const Coord entityBaseTargetCoord = getEntityBaseTargetCoord(dangerousEnemyMonster, enemyBaseCoords);

                // вражеские деферы
                auto enemiesAttackedEntityVec = getEnemiesAttackedEntity(enemies, dangerousEnemyMonster, entityBaseTargetCoord);
                cerr << "Visible defender-units nb for attacker: " << enemiesAttackedEntityVec.size() << endl;

                // если вражеский юнит 1, то контрим его
                if (auto act = tryAttackerControlEnemyDefender(enemiesAttackedEntityVec, dangerousEnemyMonster); act.has_value()) return act.value();

                // пробуем задуть если деферы не помешают
                if (auto act = tryAttackerWindMonster(enemiesAttackedEntityVec, entityBaseTargetCoord, dangerousEnemyMonster); act.has_value()) return act.value();

                // пробуем навесить щит на монстров, которые сами пушат врага
                if (auto act = tryAttackerShieldOtherPushingMonsters(enemyDangerousMonsters); act.has_value()) return act.value();
            }

            // ищем куда пойти для поиска монстров-пушеров
            return tryAttackerGoToNextPos();
        }
    }

    Actions prepairActions() {

        totalCurTreatment = 0;
        vector<Entity> ourDangerousMonsters, enemyDangerousMonsters, monstersToFarmingAndEnemyUnits,
            safetyOur(our.begin(), our.end());
        set<int> ourDangerousAccountedMonsters, // учтенные в расчете дистанции до базы (попадают в этот сет, когда расстояние до базы <= 400)
            enemyDangerousAccountedMonsters,
            accountedEnemyUnits;


        // если проходим через точки спавна монстров, то сбрасываем соответствующие счетчики
        for (Entity& ourEnt : our) {
            if (dist(ourEnt.coords, bottomWaitCoord) < 200.) {
                bottomWaitCoordRotting = 0;
            }
            if (dist(ourEnt.coords, topWaitCoord) < 200.) {
                topWaitCoordRotting = 0;
            }
        }

        for (Entity& monster : monsters) {
            monster.simulatedCoords = monster.coords;
        }

        // заносим опасных вражеских юнитов в вектор с монстрами
        // {
        //     for (int i = 0; i < enemies.size(); i++) {
        //         double curStepDist = dist(enemies[i].coords, ourBaseCoords),
        //             nextStepDist = dist({ enemies[i].coords.x + enemies[i].vx, enemies[i].coords.y + enemies[i].vy }, ourBaseCoords);
        // 
        //         // если враг на нашей половине и идет В СТОРОНУ нашей базы
        //         if ((curStepDist < cMaxDist / 2) && (nextStepDist < curStepDist)) {
        //             // cerr << "Enemy " << enemies[i].id << " is danger for our base" << endl;
        //             monsters.push_back(enemies[i]);
        //         }
        //     }
        // }


        // симуляция движений видимых сущностей
        {
            double curDistToOurBase, curDistToEnemyBase;
            for (int simulationDepth = 1; simulationDepth < cMaxSimulationDepth; simulationDepth++) {
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
                            // запоминаем координату, на которой монстр заходит в радиус
                            if (monster.baseRadiusReachCoord.x == cInvalidCoord.x) {
                                monster.baseRadiusReachCoord = monster.simulatedCoords;
                            }
                            int dBaseX = ourBaseCoords.x - monster.simulatedCoords.x, // важно чтобы значения могли быть отрицательным
                                dBaseY = ourBaseCoords.y - monster.simulatedCoords.y; // для правильных значений компонент скорости
                            monster.vx = ceil(cMonsterSpeed * dBaseX / curDistToOurBase);
                            monster.vy = ceil(cMonsterSpeed * dBaseY / curDistToOurBase);
                        }
                        if (curDistToEnemyBase < cBaseRadius)
                        {
                            // запоминаем координату, на которой монстр заходит в радиус
                            if (monster.baseRadiusReachCoord.x == cInvalidCoord.x) {
                                monster.baseRadiusReachCoord = monster.simulatedCoords;
                            }
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
                            && !ourDangerousAccountedMonsters.count(monster.id)) {
                            monster.importance = (cMaxDist / 2. - dist(monster.coords, ourBaseCoords)) * 3500.;
                            monster.neededUnitsNb = 1;

                            // cerr << "Enemy " << monster.id << " importance: " << monster.importance << endl;

                            ourDangerousAccountedMonsters.insert(monster.id);
                            ourDangerousMonsters.push_back(monster);
                        }
                    }

                    // учет положений и состояний пауков
                    {
                        // добавляем дистанцию к общей дистанции до базы за тот ход, который симулируем
                        if (!ourDangerousAccountedMonsters.count(monster.id)
                            && !enemyDangerousAccountedMonsters.count(monster.id)) {
                            monster.distanceToReachBase += cMonsterSpeed;
                            monster.stepsToBase++;


                            // ОАСНЫЕ ДЛЯ НАШЕЙ БАЗЫ
                            // если по результатам симуляции монстр дошел до базы - считаем его опасным
                            if (curDistToOurBase < cDistanceThresholdWhenEnemyReachBase
                                && monster.distanceToReachBase < cDangerousDistanceThreshold
                                && monster.baseRadiusReachCoord.x >= 0 && monster.baseRadiusReachCoord.x <= cMaxCoord.x
                                && monster.baseRadiusReachCoord.y >= 0 && monster.baseRadiusReachCoord.y <= cMaxCoord.y)
                            {
                                monster.importance = (cMaxSimulationDepth - simulationDepth - 1) * cImportanceStep;

                                bool needAdditionalUnit = (simulationDepth < 8) || monster.shieldLife;
                                monster.neededUnitsNb = static_cast<int>(std::ceil(static_cast<double>(monster.health) / (monster.stepsToBase * cUnitDamage))) + (needAdditionalUnit ? 1 : 0);

                                ourDangerousAccountedMonsters.insert(monster.id);
                                ourDangerousMonsters.push_back(monster);
                            }


                            // ОПАСНЫЕ ДЛЯ ВРАЖЕСКОЙ БАЗЫ
                            {
                                // флаг, означающий, что монстр сам атакует базу и ему можно будет помочь
                                bool selfAttackedEnemyBase =
                                    (curDistToEnemyBase < cDistanceThresholdWhenEnemyReachBase) // если монстр все же достигнет вражеской базы
                                    && (monster.distanceToReachBase < cDangerousDistanceEnemyThreshold) // и если это будет в обозримом будущем
                                    && (monster.shieldLife <= 1) // монстр без щита; // и нет щитка
                                    && monster.baseRadiusReachCoord.x >= 0 && monster.baseRadiusReachCoord.x <= cMaxCoord.x
                                    && monster.baseRadiusReachCoord.y >= 0 && monster.baseRadiusReachCoord.y <= cMaxCoord.y;
                                // cerr << "Entity " << monster.id << " is selfAttackedEnemyBase: " << selfAttackedEnemyBase << endl;

                                // если паук сам движется к базе врага (угол такой, что он зайдет в радиус вражеской базы)
                                if (selfAttackedEnemyBase) {
                                    monster.importance = (cMaxSimulationDepth - simulationDepth - 1) * cImportanceStep;
                                    enemyDangerousAccountedMonsters.insert(monster.id);
                                    enemyDangerousMonsters.push_back(monster);
                                }
                                // флаг, означающий, что монстра можно сдуть с ТЕКУЩЕЙ позиции в радиус
                                bool canBeMovedToEnemyBase =
                                    (simulationDepth <= 4)
                                    && (curDistToEnemyBase < cBaseRadius)
                                    && (monster.shieldLife <= 1) // монстр без щаита
                                    && (monster.health > 2); // не будет убит в ближайшие 1-2 хода

                                if (canBeMovedToEnemyBase) {
                                    // cerr << "Entity " << monster.id << " is canBeMovedToEnemyBase: " << selfAttackedEnemyBase << endl;
                                    cerr << "Save entity " << monster.id << " as canBeMovedToEnemyBase" << endl;
                                    double distanceBetweenAttackerAndMonster = dist(monster.simulatedCoords, our[Attacker].coords);
                                    monster.importance = (10000. - distanceBetweenAttackerAndMonster - dist(monster.coords, enemyBaseCoords)) * cImportanceStep;
                                    // monster.needWind = true;
                                    enemyDangerousAccountedMonsters.insert(monster.id);
                                    enemyDangerousMonsters.push_back(monster);
                                }
                            }
                        }
                    }
                }
            }
        }

        // учет монстров, которых можно пофармить
        {
            for (Entity& monster : monsters) {
                double curDistToOurBase = dist(monster.coords, ourBaseCoords);
                if (!ourDangerousAccountedMonsters.count(monster.id) // если монстр не опасный для нас
                    && curDistToOurBase < 8500. // считаем пригодным для фарма, если он проходит от НАШЕЙ базы на расстоянии <8500
                    && monster.type == Monster) {
                    // важность монстра считается по его ТЕКУЩЕМУ положению
                    monster.importance = (cMaxDist - curDistToOurBase) * 2500. / cMaxDist;
                    monstersToFarmingAndEnemyUnits.push_back(monster);
                }
            }
        }

        // сортировка по важности
        {
            sort(ourDangerousMonsters.begin(), ourDangerousMonsters.end(), entityCompare);
            sort(enemyDangerousMonsters.begin(), enemyDangerousMonsters.end(), entityCompare);
            sort(monstersToFarmingAndEnemyUnits.begin(), monstersToFarmingAndEnemyUnits.end(), entityCompare);
        }

        // формируем ходы
        Actions actions;
        actions.resize(3);
        actions[Attacker] = getAttackerAction(enemyDangerousMonsters);
        {
            vector<Entity> ourAvailableUnits{ our[Defender], our[Farmer] };

            // cerr << "Create actions for Defender and Farmer" << endl;
            for (Entity& monster : ourDangerousMonsters) {

                for (int neededUnitIdx = 0; neededUnitIdx < min(int(ourAvailableUnits.size()), monster.neededUnitsNb); neededUnitIdx++) {
                    // ищем ближайшего юнита к данному монстру
                    int bestOurUnitId = convertToArrayIdx(getIdxOfNearestUnits(monster.coords, ourAvailableUnits));
                    // cerr << "\tBest our unit for dangerous entity " << monster.id << " is " << bestOurUnitId << endl;

                    // если юнит ближе к базе чем монстр И дальность до монстра меньша дальности ветерка И дальность от базы < 7500 (обдумать эвристику)
                    // то пытаемся выдувать монстра подальше
                    double bestUnitDistToBase = dist(our[bestOurUnitId].coords, ourBaseCoords),
                        monsterDistToBase = dist(monster.coords, ourBaseCoords),
                        distBetweenUnitAndMonster = dist(monster.coords, our[bestOurUnitId].coords);

                    // отдельно обрабатываем кейс с контром врага
                    if (monster.type == Enemy
                        && bases.ourBase.mana >= 10
                        && !monster.shieldLife) {
                        // cerr << "Monster type Enemy" << endl;
                        // cerr << "\tEnemy dangerous entity, bestUnitDistToBase:" << bestUnitDistToBase << ", monsterDistToBase:" << monsterDistToBase << endl;
                        // cerr << "\t\tdistBetweenUnitAndMonster:" << distBetweenUnitAndMonster << endl;
                        if (distBetweenUnitAndMonster < cControlSpelDistance) {
                            bases.ourBase.mana -= 10;
                            actions[bestOurUnitId] = { "SPELL CONTROL", monster.id, enemyBaseCoords.x, enemyBaseCoords.y };
                        }
                        // иначе идем с опережением врага
                        else {
                            actions[bestOurUnitId] = { "MOVE", monster.coords.x + monster.vx, monster.coords.y + monster.vy };
                        }
                    }
                    // обрабатываем монстров
                    else if (distBetweenUnitAndMonster < cWindSpellRadius // если ветереком можно сдуть монстра
                        && monsterDistToBase < getWindCastAllowRadius()
                        && !monster.shieldLife // нет смысла сдувать монстров с щитом
                        && neededUnitIdx == 0 // сдувом будет заниматься только ближайший юнит
                        && bases.ourBase.mana >= 15
                        ) {
                        bases.ourBase.mana -= 10;
                        actions[bestOurUnitId] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                    }
                    // иначе идем с опережением монстра
                    else {
                        // cerr << "Monster type Monster, go forward him: MOVE " << monster.coords.x + monster.vx << ", " << monster.coords.y + monster.vy << endl;
                        actions[bestOurUnitId] = { "MOVE", monster.coords.x + monster.vx, monster.coords.y + monster.vy };
                    }

                    // удаляем выбранного юнита из доступных для хода
                    eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);

                    if (ourAvailableUnits.empty()) return actions;
                }
                if (ourAvailableUnits.empty()) return actions;
            }
            cerr << "Dangerous processed" << endl;


            if (ourAvailableUnits.size()) {

                // каждые 6 ходов надо проверять точки, через которые проходят пауки из спавнов к нам на базу
                // если в этих зонах нет опасных пауков, то можно фармить дальше из списка 
                if (topWaitCoordRotting >= 6 || bottomWaitCoordRotting >= 6) {

                    // проверяем topWaitCoordRotting
                    if (topWaitCoordRotting >= 6) {
                        int bestOurUnitId = convertToArrayIdx(getIdxOfNearestUnits(topWaitCoord, ourAvailableUnits));

                        actions[bestOurUnitId] = { "MOVE", topWaitCoord.x, topWaitCoord.y };

                        eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);
                        if (ourAvailableUnits.empty()) return actions;
                    }

                    // проверяем topWaitCoordRotting
                    if (topWaitCoordRotting >= 6) {
                        int bestOurUnitId = convertToArrayIdx(getIdxOfNearestUnits(bottomWaitCoord, ourAvailableUnits));

                        actions[bestOurUnitId] = { "MOVE", bottomWaitCoord.x, bottomWaitCoord.y };

                        eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);
                        if (ourAvailableUnits.empty()) return actions;
                    }
                }

                // если есть неопасные монстры - идем фармить
                if (monstersToFarmingAndEnemyUnits.size() && turnCnt < cFarmingPeriod) {

                    for (const Entity& undangerousMonster : monstersToFarmingAndEnemyUnits) {
                        // ищем ближайшего юнита к данному монстру
                        int bestOurUnitId = convertToArrayIdx(getIdxOfNearestUnits(undangerousMonster.coords, ourAvailableUnits));

                        // добавляем команду на атаку ближайшим юнитом этого монстра
                        actions[bestOurUnitId] = { "MOVE", undangerousMonster.coords.x + undangerousMonster.vx, undangerousMonster.coords.y + undangerousMonster.vy };

                        eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);
                        if (ourAvailableUnits.empty()) return actions;
                    }
                }
                cerr << "Undangerous processed" << endl;

                for (int i = 0; i < ourAvailableUnits.size(); i++) {
                    cerr << "Creating wait actions for our unit " << ourAvailableUnits[i].id << endl;
                    actions[convertToArrayIdx(ourAvailableUnits[i].id)] =
                    { "MOVE", waitCoordsVec2[ourAvailableUnits.size() - 1][i].x, waitCoordsVec2[ourAvailableUnits.size() - 1][i].y };
                }
            }
        }

        bottomWaitCoordRotting++;
        topWaitCoordRotting++;
        return actions;
    }

    Coord getMinimalAttackCoord() {
        return {
            abs(enemyBaseCoords.x - static_cast<int>(ceil((300 + cHeroVisibleAreaRadius) * cos(45. * M_PI / 180.)))),
            abs(enemyBaseCoords.y - static_cast<int>(ceil((300 + cHeroVisibleAreaRadius) * sin(45. * M_PI / 180.))))
        };
    }

    vector<Coord> getAttackDefaultCoords() {
        static const double attackRadiusCoef = 1.;
        return {
            Coord{  abs(enemyBaseCoords.x - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * cos(20. * M_PI / 180.)))),
                    abs(enemyBaseCoords.y - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * sin(20. * M_PI / 180.)))) },
            Coord{  abs(enemyBaseCoords.x - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * cos(70. * M_PI / 180.)))),
                    abs(enemyBaseCoords.y - static_cast<int>(ceil(cBaseRadius * attackRadiusCoef * sin(70. * M_PI / 180.)))) }
        };
    }

    vector<Coord> getAttackerFarmCoords() {
        return {
            Coord{ cMaxCoord.x / 2, static_cast<int>(cHeroVisibleAreaRadius) },
            Coord{ cMaxCoord.x / 2, cMaxCoord.y - static_cast<int>(cHeroVisibleAreaRadius) }
        };
    }

    vector<vector<Coord>> getWaitCoords() {
        static const double defendRadiusCoef = 1.3;
        return {
            vector<Coord> {
                Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(45. * M_PI / 180.)))),
                        abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(45. * M_PI / 180.)))) }
            },
            vector<Coord> { bottomWaitCoord, topWaitCoord}
                // vector<Coord> {
                //     Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(20. * M_PI / 180.)))),
                //             abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(20. * M_PI / 180.)))) },
                //     Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * cos(70. * M_PI / 180.)))),
                //             abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoef * sin(70. * M_PI / 180.)))) }
                // }
        };
    }

    void updateWaitCoords() {
        static const double waitRadiusCoef = 1.42; // or 1.64 for full attack
        bottomWaitCoord = { abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * waitRadiusCoef * cos(22.5 * M_PI / 180.)))),
                            abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * waitRadiusCoef * sin(22.5 * M_PI / 180.)))) };
        topWaitCoord = { abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * waitRadiusCoef * cos(67.5 * M_PI / 180.)))),
                         abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * waitRadiusCoef * sin(67.5 * M_PI / 180.)))) };
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