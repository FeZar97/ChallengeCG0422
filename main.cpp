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
const double cBaseVisibilityRadius{ 6000 };
const Coord cMaxCoord{ 17630, 9000 };
const Coord cInvalidCoord{ -17630, -9000 };
const double cMaxDist = dist({ 0, 0 }, cMaxCoord);
const int cMonsterSpeed = 400;
const int cUnitSpeed = 800;
const int cUnitDamage = 2;
int turnCnt = 0;
const int cFarmingPeriod = 115; // количество ходов в начале игры, в течение которого фармим
const double cBaseAttackRadius = 300.;
const int cMaxTurnIdx = 219;
const int cTurnWhileEnemyAtackerIsNotImportnant = 55;
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
    Coord baseRadiusReachCoord{ cInvalidCoord }; // координата, в которой монстр достигает радиуса базы. если эта координата за пределами поля - монстр не интересует

    bool needWind{ false }; // нужно ли применять wind на монстра

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
    for (auto iter = vec.begin(); iter != vec.end(); iter++) {
        if (iter->id == id) {
            vec.erase(iter);
            return;
        }
    }
}
int getWindCastAllowRadius() {
    return 4500; // +1000 * bases.ourBase.mana / 40;
}
bool isEntityInCoord(const Entity& entity, const Coord& coord, int availableRaius = 0) {
    return dist(entity.coords, coord) < (2 + availableRaius);
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
    bool isHighThreatment{ false }; // если нашу базу пушат 2 или 3 врага

    Coord minimalAttackCoord;
    vector<Coord> attackCoordsVec, attackerFarmCoordVec;
    vector<vector<Coord>> waitCoordsVec2;
    vector<vector<Coord>> waitCoordsVec2ForHightThreatment;

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
        waitCoordsVec2ForHightThreatment = getWaitCoordsForHighThreatments();
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
        bool isAttackerInWatchPos0 = isEntityInCoord(our[Attacker], attackCoordsVec[0], 200);
        bool isAttackerInWatchPos1 = isEntityInCoord(our[Attacker], attackCoordsVec[1], 200);

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
    optional<Action> tryAttackerStartFarming(vector<Entity>& ourDangerousMonsters) {
        if (turnCnt > cFarmingPeriod) {
            return nullopt;
        }

        cerr << "---tryAttackerStartFarming---" << endl;

        // найти ближайшего монстра из всех, у которого расстояние до нащей базы > cDangerousDistanceThreshold 
        // (того расстояния, на котором монсстр считается опасным для нашей базы)
        double minDistToMonster = cMaxDist;
        Entity* bestEntToAttack = nullptr;

        // сначала ищем среди опасных для нашей базы
        for (Entity& ent : ourDangerousMonsters) {
            if (dist(ent.coords, our[Attacker].coords) < minDistToMonster
                && dist(ent.coords, ourBaseCoords) > 9000.
                ) {
                minDistToMonster = dist(ent.coords, our[Attacker].coords);
                bestEntToAttack = &ent;
            }
        }
        // затем по необходимости пробежимся по всем остальным мобам
        if (!bestEntToAttack) {
            for (Entity& ent : monsters) {
                if (dist(ent.coords, our[Attacker].coords) < minDistToMonster
                    && dist(ent.coords, ourBaseCoords) > 9000.
                    ) {
                    minDistToMonster = dist(ent.coords, our[Attacker].coords);
                    bestEntToAttack = &ent;
                }
            }
        }

        if (bestEntToAttack) return Action{ "MOVE", bestEntToAttack->coords.x + bestEntToAttack->vx, bestEntToAttack->coords.y + bestEntToAttack->vy };

        // ищем куда пойти
        bool isAttackerInFarmPos0 = isEntityInCoord(our[Attacker], attackerFarmCoordVec[0], 200);
        bool isAttackerInFarmPos1 = isEntityInCoord(our[Attacker], attackerFarmCoordVec[1], 200);

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
   
    void getAttackerAction(Actions& actions, vector<Entity>& ourDangerousMonsters, vector<Entity>& enemyDangerousMonsters) {
        double attackerDistToBase = dist(our[Attacker].coords, enemyBaseCoords);

        if (auto act = tryAttackerStartFarming(ourDangerousMonsters); act.has_value()) {
            actions[Attacker] = act.value();
            return;
        }

        // !!! в enemyDangerousMonsters монстры без щитков !!!

        cerr << "Enemy Dangerous Monsters is empty: " << (enemyDangerousMonsters.empty() ? "true" : "false") << endl;
        cerr << "PushMonsterId: " << PushMonsterId << endl;

        if (enemyDangerousMonsters.empty()
            || PushMonsterId) {
            // если ЛИБО список монстров для атаки пустой
            // ЛИБО был запушенный монстр

            set<int> accountedPushMonsterEnemyBase;

            // если есть пушащий монстр из предыдущих ходов
            if (Entity* pushedEntityPtr = getEntityById(enemyDangerousMonsters, PushMonsterId); pushedEntityPtr) {
                // пробуем помочь монстру
                if (auto act = TryAttackerHelpToPushingMonster(pushedEntityPtr, enemyDangerousMonsters); act.has_value()) {
                    actions[Attacker] = act.value();
                    return;
                }

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
                        if (auto act = TryAttackerHelpToPushingMonster(&ent, enemyDangerousMonsters); act.has_value()) {
                            actions[Attacker] = act.value();
                            return;
                        }

                        // иначе монстр не интересен
                        accountedPushMonsterEnemyBase.insert(ent.id);
                        break;
                    }
                }
            }
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
                if (auto act = tryAttackerControlEnemyDefender(enemiesAttackedEntityVec, dangerousEnemyMonster); act.has_value()) {
                    actions[Attacker] = act.value();
                    return;
                }

                // пробуем задуть если деферы не помешают
                if (auto act = tryAttackerWindMonster(enemiesAttackedEntityVec, entityBaseTargetCoord, dangerousEnemyMonster); act.has_value()) {
                    actions[Attacker] = act.value();
                    return;
                }

                // пробуем навесить щит на монстров, которые сами пушат врага
                if (auto act = tryAttackerShieldOtherPushingMonsters(enemyDangerousMonsters); act.has_value()) {
                    actions[Attacker] = act.value();
                    return;
                }
            }
        }

        // ищем куда пойти для поиска монстров-пушеров
        actions[Attacker] = tryAttackerGoToNextPos();
    }

    // ------------------------- DEFENDER -------------------------
    void tryDefendersAttackDangerousMonster(Actions& actions, vector<Entity>& availableUnits, vector<Entity>& dangerousMonsters) {
        cerr << "---tryDefendersAttackDangerousMonster---" << endl;

        for (Entity& monster : dangerousMonsters) {

            double monsterDistToBase = dist(monster.coords, ourBaseCoords);

            // если высокая угроза нападения - проверяем только монстров в радиусе 6000 от базы
            if (isHighThreatment && monsterDistToBase > cBaseVisibilityRadius) {
                continue;
            }

            cerr << "Try handle dangerous monster " << monster.id << endl;

            for (int neededUnitIdx = 0; neededUnitIdx < min(int(availableUnits.size()), monster.neededUnitsNb); neededUnitIdx++) {
                // ищем ближайшего юнита к данному монстру
                int bestOurUnitId = getIdxOfNearestUnits(monster.coords, availableUnits);
                cerr << "\t\t Best our unit for dangerous entity " << monster.id << " is " << bestOurUnitId << endl;

                double distBetweenUnitAndMonster = dist(monster.coords, our[convertToArrayIdx(bestOurUnitId)].coords);

                // если дальность от базы < 7500 (обдумать эвристику)
                // то пытаемся выдувать монстра подальше
                if (distBetweenUnitAndMonster < cWindSpellRadius // если ветереком можно сдуть монстра
                    && monsterDistToBase < getWindCastAllowRadius()
                    && !monster.shieldLife // нет смысла сдувать монстров с щитом
                    && neededUnitIdx == 0 // сдувом будет заниматься только ближайший юнит
                    && bases.ourBase.mana >= 10
                    ) {
                    cerr << "\t\t Our unit " << bestOurUnitId << " will WIND out monster " << monster.id << endl;
                    bases.ourBase.mana -= 10;
                    actions[convertToArrayIdx(bestOurUnitId)] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                    monster.neededUnitsNb--;

                    // удаляем выбранного юнита из доступных для хода
                    eraseFromVecUnitId(bestOurUnitId, availableUnits);
                    if (availableUnits.empty()) return;

                    // если монстра сдули, то другими юнитами не надо пытаться его атаковать, переходим к следующим юнитам
                    break;
                }
                // иначе идем с опережением монстра
                else {
                    cerr << "\t\t Our unit " << bestOurUnitId << " will go forward to monster " << monster.id << endl;
                    actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", monster.coords.x + monster.vx, monster.coords.y + monster.vy };
                    // удаляем выбранного юнита из доступных для хода
                    eraseFromVecUnitId(bestOurUnitId, availableUnits);
                    if (availableUnits.empty()) return;
                }
            }
            if (availableUnits.empty()) return;
        }
    }
    void tryDefendersFarming(Actions& actions, vector<Entity>& availableUnits, vector<Entity>& monstersToFarming) {
        cerr << "---tryDefendersFarming---" << endl;

        if (availableUnits.empty() || monstersToFarming.empty()) {
            return;
        }

        cerr << "\t\t availableUnits size: " << availableUnits.size() << endl;
        cerr << "\t\t monstersToFarming size: " << monstersToFarming.size() << endl;

        while (!availableUnits.empty() && !monstersToFarming.empty()) {
            // каждые 6 ходов надо проверять точки, через которые проходят пауки из спавнов к нам на базу
            // если в этих зонах нет опасных пауков, то можно фармить дальше из списка 
            if (topWaitCoordRotting >= 6 || bottomWaitCoordRotting >= 6) {
            
                // проверяем topWaitCoordRotting
                if (topWaitCoordRotting >= 6) {
                    int bestOurUnitId = getIdxOfNearestUnits(topWaitCoord, availableUnits);

                    cerr << "\t\t Our unit " << bestOurUnitId << " will go to FARM in topWaitCoord" << endl;
                    actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", topWaitCoord.x, topWaitCoord.y };
            
                    eraseFromVecUnitId(bestOurUnitId, availableUnits);
                    if (availableUnits.empty()) return;
                    else continue;
                }
            
                // проверяем topWaitCoordRotting
                if (bottomWaitCoordRotting >= 6) {
                    int bestOurUnitId = getIdxOfNearestUnits(bottomWaitCoord, availableUnits);

                    cerr << "\t\t Our unit " << bestOurUnitId << " will go to FARM in bottomWaitCoordRotting" << endl;
                    actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", bottomWaitCoord.x, bottomWaitCoord.y };
            
                    eraseFromVecUnitId(bestOurUnitId, availableUnits);
                    if (availableUnits.empty()) return;
                    else continue;
                }
            }

            for (const Entity& undangerousMonster : monstersToFarming) {
                // ищем ближайшего юнита к данному монстру
                int bestOurUnitId = getIdxOfNearestUnits(undangerousMonster.coords, availableUnits);

                cerr << "\t\t Our unit " << bestOurUnitId << " will go to FARM undangerous monster " << undangerousMonster.id << endl;
                actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", undangerousMonster.coords.x + undangerousMonster.vx, undangerousMonster.coords.y + undangerousMonster.vy };
                eraseFromVecUnitId(bestOurUnitId, availableUnits);

                eraseFromVecUnitId(bestOurUnitId, availableUnits);
                if (availableUnits.empty()) return;
                else continue;
            }
        }
    }
    void tryDefendersWaiting(Actions& actions, vector<Entity>& availableUnits) {
        cerr << "---tryDefendersWaiting---" << endl;

        int waitCoordIdx = 0;
        int waitCoordsSetIdx = availableUnits.size() - 1;
        cerr << "\t\t isHighThreatment: " << (isHighThreatment ? "true" : "false") << endl;
        
        while (!availableUnits.empty()) {

            Coord targetCoords = (isHighThreatment ? waitCoordsVec2ForHightThreatment[waitCoordsSetIdx][waitCoordIdx] : waitCoordsVec2[waitCoordsSetIdx][waitCoordIdx]);
            // cerr << "\t\t Target wait coord for unit " << availableUnits[0].id << " is (" << targetCoords.x << ", " << targetCoords.y << ")" << endl;
            actions[convertToArrayIdx(availableUnits[0].id)] = { "MOVE", targetCoords.x, targetCoords.y };

            waitCoordIdx = (waitCoordIdx + 1) % (waitCoordsSetIdx + 1);
            eraseFromVecUnitId(availableUnits[0].id, availableUnits);
            if (availableUnits.empty()) return;
            else continue;
        }
    }
    
    void getDefendersActions(Actions& actions, vector<Entity>& ourAvailableUnits, vector<Entity>& ourDangerousMonsters, vector<Entity>& enemyAttackers, vector<Entity>& ourMonstersToFarming) {

        cerr << "Create actions for Defender and Farmer, ourAvailableUnits size: " << ourAvailableUnits.size() << endl;
        cerr << "\t\t Dangerous Monsters size: " << ourDangerousMonsters.size() << endl;
        cerr << "\t\t Enemy Attacker size: " << enemyAttackers.size() << endl;


        isHighThreatment = false;
        if (enemyAttackers.size() == 0 || turnCnt < cTurnWhileEnemyAtackerIsNotImportnant)
        {
            // если атакеров нет, то играем дефолтно
            tryDefendersAttackDangerousMonster(actions, ourAvailableUnits, ourDangerousMonsters);
            tryDefendersFarming(actions, ourAvailableUnits, ourMonstersToFarming);
            tryDefendersWaiting(actions, ourAvailableUnits);
        }
        // если базу атакует 2 или 3 вражеских юнита - идем в жесткий деф
        else if (enemyAttackers.size() >= 2) {
            isHighThreatment = true;
            // если базу атакует 2 или 3 вражеских юнита - идем в жесткий деф
            // при этом атакер должен максимально быстро запушить монстров к врагу в радиус базы
            tryDefendersAttackDangerousMonster(actions, ourAvailableUnits, ourDangerousMonsters);
            tryDefendersWaiting(actions, ourAvailableUnits);
        }
        // если всего 1 вражеский атакер
        else if (enemyAttackers.size() == 1) {
            // если базу атакует 1 юнит, то надо выяснить есть ли монстры в радиусе 6000 от базы
            //     если есть 
            //        то на самого опасного монстра направляем ближайшего юнита
            //        а вторым юнитом начинаем контролить атакера
            //     если монстров нет
            //           то одним юнитом надо приблизиться к вражескому атакеру и искать монстров где то возле атакера и не позволять пушить монстров к нам на базу
            //           а для второго юнита скоратить максимальную дистанцию от базы на которой можно фармить
            Entity& enemyAttacker = enemyAttackers.at(0);

            // проверяем самого опасного монстра
            if (ourDangerousMonsters.size()) {

                // первым контролим самого опасного монстра
                {
                    Entity& mostDangerousMonster = ourDangerousMonsters.front();

                    // ищем ближайшего юнита к данному монстру
                    int bestOurUnitId = getIdxOfNearestUnits(mostDangerousMonster.coords, ourAvailableUnits);
                    double distBetweenOurUnitAndMonster = dist(mostDangerousMonster.coords, our[convertToArrayIdx(bestOurUnitId)].coords);

                    cerr << "\t\t ATTACK Most Dangerous Monster " << mostDangerousMonster.id << ", our nearest unit: " << bestOurUnitId << endl;

                    if (mostDangerousMonster.needWind // должен быть сдут
                        && !mostDangerousMonster.shieldLife // нет щитка
                        && bases.ourBase.mana >= 10 // есть мана
                        && distBetweenOurUnitAndMonster < cWindSpellRadius // расстояния хватает
                        ) {
                        cerr << "\t\t Defender " << bestOurUnitId << " can WIND monster " << mostDangerousMonster.id << endl;
                        bases.ourBase.mana -= 10;
                        actions[convertToArrayIdx(bestOurUnitId)] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                    }
                    else {
                        // иначе идем с опережением монстра
                        cerr << "\t\t Defender " << bestOurUnitId << " will go forward to monster " << mostDangerousMonster.id << endl;
                        actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", mostDangerousMonster.coords.x + mostDangerousMonster.vx, mostDangerousMonster.coords.y + mostDangerousMonster.vy };
                    }

                    // удаляем выбранного юнита из доступных для хода
                    eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);
                    if (ourAvailableUnits.empty()) return;
                }

                // вторым пытаемся законтролить атакера
                {
                    // ищем ближайшего юнита к атакеру
                    Coord nextAttackerCoord = { enemyAttacker.coords.x + enemyAttacker.vx, enemyAttacker.coords.y + enemyAttacker.vy };
                    int bestOurUnitId = getIdxOfNearestUnits(nextAttackerCoord, ourAvailableUnits);
                    double distBetweenOurUnitAndAttacker = dist(enemyAttacker.coords, our[convertToArrayIdx(bestOurUnitId)].coords);

                    // контроль нужен только в случае, если в радиусе каста вражеского юнита есть монстры (то есть он может их на нас направить)
                    // иначе - подходим к атакеру и ищем монстров где то возле

                    // если есть такой монстр
                    if (Entity* nearestMonsterToEnemyAttacker = getEntityById(monsters, getIdxOfNearestUnits(enemyAttacker.coords, monsters)); nearestMonsterToEnemyAttacker) {
                        // если условия позволяют - контроли врага

                        double distBetweenEnemyAttackerAndMonster = dist(enemyAttacker.coords, nearestMonsterToEnemyAttacker->coords);

                        bool canBeAttackerWinded =
                            distBetweenOurUnitAndAttacker < cWindSpellRadius // хватает расстояния
                            && !enemyAttacker.shieldLife // нет щитка
                            && bases.ourBase.mana >= 10; // есть мана

                        // bool canBeAttackerControlled =
                        //     distBetweenOurUnitAndAttacker < cControlSpelDistance // хватает расстояния
                        //     && !enemyAttacker.shieldLife // нет щитка
                        //     && bases.ourBase.mana >= 10; // есть мана

                        if (distBetweenEnemyAttackerAndMonster < cWindSpellRadius
                            && canBeAttackerWinded) {
                            cerr << "\t\t Defender " << bestOurUnitId << " need WIND enemy attacker " << enemyAttacker.id << endl;
                            bases.ourBase.mana -= 10;
                            actions[convertToArrayIdx(bestOurUnitId)] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };
                        } 
                        // else if (canBeAttackerControlled) {
                        //     cerr << "\t\t Defender " << bestOurUnitId << " can CONTROL enemy attacker " << enemyAttacker.id << endl;
                        //     bases.ourBase.mana -= 10;
                        //     actions[convertToArrayIdx(bestOurUnitId)] = { "SPELL CONTROL", enemyAttacker.id, enemyBaseCoords.x, enemyBaseCoords.y };
                        // }
                        else {

                            // если у атакера есть щиток и он долгий, то попробуем сами атаковать моба
                            if (enemyAttacker.shieldLife > 1) {
                                cerr << "\t\t Defender " << bestOurUnitId << " will go ATTACK nearest monster to attacker " << enemyAttacker.id << endl;
                                actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", 
                                    nearestMonsterToEnemyAttacker->coords.x + nearestMonsterToEnemyAttacker->vx, nearestMonsterToEnemyAttacker->coords.y + nearestMonsterToEnemyAttacker->vy };
                            }
                            // иначе идем к атакеру с опережением
                            else {
                                cerr << "\t\t Defender " << bestOurUnitId << " will go forward to attacker " << enemyAttacker.id << endl;
                                actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", enemyAttacker.coords.x + enemyAttacker.vx, enemyAttacker.coords.y + enemyAttacker.vy };
                            }
                        }
                    }
                    else {
                        // иначе просто идем в сторону атакера с опережением
                        cerr << "\t\t Defender " << bestOurUnitId << " will go forward to attacker " << enemyAttacker.id << endl;
                        actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", enemyAttacker.coords.x + enemyAttacker.vx, enemyAttacker.coords.y + enemyAttacker.vy };
                    }

                    // удаляем выбранного юнита из доступных для хода
                    eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);
                    if (ourAvailableUnits.empty()) return;
                }

            }
            else {
                // если монстров нет
                //       то одним юнитом надо приблизиться к вражескому атакеру и искать монстров где то возле атакера и не позволять пушить монстров к нам на базу
                //       а для второго юнита скоратить максимальную дистанцию от базы на которой можно фармить

                // первым пытаемся законтролить атакера
                {
                    // ищем ближайшего юнита к атакеру
                    int bestOurUnitId = getIdxOfNearestUnits(enemyAttacker.coords, ourAvailableUnits);
                    double distBetweenOurUnitAndAttacker = dist(enemyAttacker.coords, our[convertToArrayIdx(bestOurUnitId)].coords);

                    // если маны с запасом
                    // то пытаемся увести вражеского атакера с нашей базы ВЕТЕРКОМ
                    double distBetweenUnitAndMonster = dist(enemyAttacker.coords, our[convertToArrayIdx(bestOurUnitId)].coords);

                    if (distBetweenUnitAndMonster < cWindSpellRadius // если ветереком можно сдуть монстра
                        && !enemyAttacker.shieldLife // нет смысла сдувать монстров с щитом
                        && bases.ourBase.mana >= 15
                        ) {
                        cerr << "\t\t Our unit " << bestOurUnitId << " will WIND out enemy attacker " << enemyAttacker.id << endl;
                        bases.ourBase.mana -= 10;
                        actions[convertToArrayIdx(bestOurUnitId)] = { "SPELL WIND", enemyBaseCoords.x, enemyBaseCoords.y };

                        eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);
                        if (ourAvailableUnits.empty()) return;
                    }
                    // если маны мало
                    // то просто идем в сторону атакера
                    else {
                        cerr << "\t\t Defender " << bestOurUnitId << " will go forward to attacker " << enemyAttacker.id << endl;
                        actions[convertToArrayIdx(bestOurUnitId)] = { "MOVE", enemyAttacker.coords.x + enemyAttacker.vx, enemyAttacker.coords.y + enemyAttacker.vy };
                        // удаляем выбранного юнита из доступных для хода
                        eraseFromVecUnitId(bestOurUnitId, ourAvailableUnits);
                        if (ourAvailableUnits.empty()) return;
                    }

                    // проверяем наличие монстров вблизи вражеского атакера, если такие есть, то ВЫДУВАЕМ атакера
                    if (ourDangerousMonsters.size() || ourMonstersToFarming.size()) {

                        // если возле атакера есть монстры
                        int nearestDangerousToAttackerIdx = getIdxOfNearestUnits(enemyAttacker.coords, ourDangerousMonsters);
                        int nearestFarmingToAttackerIdx = getIdxOfNearestUnits(enemyAttacker.coords, ourMonstersToFarming);
                    }
                }

                // вторым фармим
                {
                    tryDefendersAttackDangerousMonster(actions, ourAvailableUnits, ourDangerousMonsters);
                    tryDefendersFarming(actions, ourAvailableUnits, ourMonstersToFarming);
                    tryDefendersWaiting(actions, ourAvailableUnits);
                }
            }
        }
    }

    // ------------------------- PREPAIR ACTIONS -------------------------
    Actions prepairActions() {

        totalCurTreatment = 0;

        vector<Entity> ourDangerousMonsters, enemyDangerousMonsters, ourMonstersToFarming, enemyAttackers,
            safetyOur(our.begin(), our.end());
        set<int> ourDangerousAccountedMonsters, // учтенные в расчете дистанции до базы (попадают в этот сет, когда расстояние до базы <= 400)
            enemyDangerousAccountedMonsters,
            accountedEnemyUnits,
            enemyAttackersAccounted;

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

        // вычисление угрозы для базы
        // for (Entity& monster : monsters) {
        //     Coord ourBaseEntityTarget = getEntityBaseTargetCoord(monster, ourBaseCoords);
        // }

        // проверяем наличие вражеских пушеров
        {
            for (Entity& ent : enemies) {
                // достаточное условие реакции на такого юнита - достижение им радиуса базы
                if (dist(ent.coords, ourBaseCoords) < cBaseVisibilityRadius + 1000.) // если близко к базе
                {
                    ent.importance = (cMaxDist / 2. - dist(ent.coords, ourBaseCoords)) * 3500.;
                    ent.neededUnitsNb = 1;

                    cerr << "Enemy Attacker " << ent.id << " is danger FOR OUR BASE, importance: " << ent.importance << endl;

                    enemyAttackers.push_back(ent);
                }
            }
        }

        // симуляция движений видимых сущностей
        {
            double curDistToOurBase, curDistToEnemyBase;
            for (int simulationDepth = 0; simulationDepth < cMaxSimulationDepth; simulationDepth++) {
                for (Entity& monster : monsters) {

                    // симулируем координаты монстра и учитываем его дистанции
                    monster.simulatedCoords.x += monster.vx;
                    monster.simulatedCoords.y += monster.vy;

                    // дистанция до баз на i-м шаге симуляция
                    Coord ourBaseEntityTarget = getEntityBaseTargetCoord(monster, ourBaseCoords);
                    Coord enemyBaseEntityTarget = getEntityBaseTargetCoord(monster, enemyBaseCoords);
                    curDistToOurBase = dist(monster.simulatedCoords, ourBaseEntityTarget);
                    curDistToEnemyBase = dist(monster.simulatedCoords, enemyBaseEntityTarget);
                    // cerr << "Monster " << monster.id << " dist to ourBase: " << curDistToOurBase << endl;

                    // меняеем вектор скорости, когда монстр заходит в радиус базы
                    {
                        if (curDistToOurBase < cBaseRadius - cBaseAttackRadius)
                        {
                            // запоминаем координату, на которой монстр заходит в радиус
                            if (monster.baseRadiusReachCoord.x == cInvalidCoord.x) {
                                monster.baseRadiusReachCoord = monster.simulatedCoords;
                                // cerr << "Monster " << monster.id << " will reach our base at coords (" << monster.baseRadiusReachCoord.x << " ," << monster.baseRadiusReachCoord.y << ")" << endl;
                            }
                            int dBaseX = ourBaseEntityTarget.x - monster.simulatedCoords.x, // важно чтобы значения могли быть отрицательным
                                dBaseY = ourBaseEntityTarget.y - monster.simulatedCoords.y; // для правильных значений компонент скорости
                            monster.vx = ceil(cMonsterSpeed * dBaseX / curDistToOurBase);
                            monster.vy = ceil(cMonsterSpeed * dBaseY / curDistToOurBase);
                        }
                        if (curDistToEnemyBase < cBaseRadius - cBaseAttackRadius)
                        {
                            // запоминаем координату, на которой монстр заходит в радиус
                            if (monster.baseRadiusReachCoord.x == cInvalidCoord.x) {
                                monster.baseRadiusReachCoord = monster.simulatedCoords;
                            }
                            int dBaseX = enemyBaseEntityTarget.x - monster.simulatedCoords.x, // важно чтобы значения могли быть отрицательным
                                dBaseY = enemyBaseEntityTarget.y - monster.simulatedCoords.y; // для правильных значений компонент скорости
                            monster.vx = ceil(cMonsterSpeed * dBaseX / curDistToEnemyBase);
                            monster.vy = ceil(cMonsterSpeed * dBaseY / curDistToEnemyBase);
                        }
                        // cerr << "On simulation " << simulationDepth << " monster " << monster.id << " speed: " << monster.vx << " " << monster.vy << endl;
                    }

                    // учет положений и состояний пауков
                    {
                        // добавляем дистанцию к общей дистанции до базы за тот ход, который симулируем

                        // если монстр еще не учтен ни для врага ни для нас
                        if (!ourDangerousAccountedMonsters.count(monster.id)
                            && !enemyDangerousAccountedMonsters.count(monster.id)) {
                            monster.distanceToReachBase += cMonsterSpeed;
                            monster.stepsToBase++;

                            // ОПАСНЫЕ ДЛЯ НАШЕЙ БАЗЫ
                            // если по результатам симуляции монстр дошел до базы - считаем его опасным
                            if (curDistToOurBase < cReachBaseAttackRadius + 2 * cCoordsInaccuracy
                                && monster.distanceToReachBase < cDangerousDistanceThreshold
                                && monster.baseRadiusReachCoord.x >= 0 && monster.baseRadiusReachCoord.x <= cMaxCoord.x  // валидность координат
                                && monster.baseRadiusReachCoord.y >= 0 && monster.baseRadiusReachCoord.y <= cMaxCoord.y) // валидность координат
                            {
                                monster.importance = (cMaxSimulationDepth - simulationDepth) * cImportanceStep;

                                bool needAdditionalUnit = (simulationDepth <= 8) || monster.shieldLife;
                                monster.neededUnitsNb = static_cast<int>(std::ceil(static_cast<double>(monster.health) / (monster.stepsToBase * cUnitDamage))) + (needAdditionalUnit ? 1 : 0);

                                // если достигнет базы очень скоро, то надо сдуть его
                                bool canBeWinded =
                                    (monster.distanceToReachBase < cBaseRadius)
                                    && (monster.shieldLife <= 1) // монстр без щаита
                                    && (monster.health > 4); // не будет убит в ближайшие 1-2 хода
                                if (canBeWinded) {
                                    monster.needWind = true;
                                }

                                cerr << "Monster " << monster.id << " is DANGER for our base, importance: " << monster.importance
                                    << ", neededUnitsNb: " << monster.neededUnitsNb
                                    << ", need WIND: " << (monster.needWind ? "true" : "false") << endl;

                                ourDangerousAccountedMonsters.insert(monster.id);
                                ourDangerousMonsters.push_back(monster);
                            }

                            // ОПАСНЫЕ ДЛЯ ВРАЖЕСКОЙ БАЗЫ
                            {
                                // флаг, означающий, что монстр сам атакует базу и ему можно будет помочь
                                bool isDangerousForEnemyBase =
                                    (curDistToEnemyBase < cReachBaseAttackRadius) // если монстр все же достигнет вражеской базы
                                    && (monster.distanceToReachBase < cDangerousDistanceEnemyThreshold) // и если это будет в обозримом будущем
                                    && (monster.shieldLife <= 1) // монстр без щита
                                    && monster.baseRadiusReachCoord.x >= 0 && monster.baseRadiusReachCoord.x <= cMaxCoord.x
                                    && monster.baseRadiusReachCoord.y >= 0 && monster.baseRadiusReachCoord.y <= cMaxCoord.y;

                                // флаг, означающий, что монстра можно сдуть с ТЕКУЩЕЙ позиции в радиус базы врага
                                bool canBeWindedToEnemyBase =
                                    (simulationDepth <= 3)
                                    && (curDistToEnemyBase <= cBaseRadius + cWindDistance)
                                    && (monster.shieldLife <= 1) // монстр без щаита
                                    && (monster.health > 2); // не будет убит в ближайшие 1-2 хода

                                // если паук сам движется к базе врага (угол такой, что он зайдет в радиус вражеской базы)
                                if (isDangerousForEnemyBase || canBeWindedToEnemyBase) {

                                    if (isDangerousForEnemyBase) {
                                        cerr << "Save monster " << monster.id << " as Dangerous For Enemy Base" << endl;
                                        monster.importance = (cMaxSimulationDepth - simulationDepth - 1) * cImportanceStep;
                                    }
                                    else {
                                        cerr << "Save monster " << monster.id << " as canBeWindedToEnemyBase" << endl;
                                        monster.needWind = true;
                                        double distanceBetweenAttackerAndMonster = dist(monster.simulatedCoords, our[Attacker].coords);
                                        monster.importance = (10000. - distanceBetweenAttackerAndMonster - dist(monster.coords, enemyBaseCoords)) * cImportanceStep;
                                    }

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
                    ourMonstersToFarming.push_back(monster);
                }
            }
        }

        // сортировка по важности
        {
            sort(ourDangerousMonsters.begin(), ourDangerousMonsters.end(), entityCompare);
            sort(enemyDangerousMonsters.begin(), enemyDangerousMonsters.end(), entityCompare);
            sort(ourMonstersToFarming.begin(), ourMonstersToFarming.end(), entityCompare);
            sort(enemyAttackers.begin(), enemyAttackers.end(), entityCompare);
        }

        // формируем ходы
        Actions actions; actions.resize(3);

        getAttackerAction(actions, ourDangerousMonsters, enemyDangerousMonsters);

        vector<Entity> ourAvailableUnits = { our[Defender], our[Farmer] };
        getDefendersActions(actions, ourAvailableUnits, ourDangerousMonsters, enemyAttackers, ourMonstersToFarming);

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
            vector<Coord> { 
                    bottomWaitCoord, 
                    topWaitCoord
            }
        };
    }

    vector<vector<Coord>> getWaitCoordsForHighThreatments() {
        static const double defendRadiusCoefHightThreatment = 0.8;
        return {
            vector<Coord> {
                Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoefHightThreatment * cos(45. * M_PI / 180.)))),
                        abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoefHightThreatment * sin(45. * M_PI / 180.)))) }
            },
            vector<Coord> {
                Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoefHightThreatment * cos(20. * M_PI / 180.)))),
                        abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoefHightThreatment * sin(20. * M_PI / 180.)))) },
                Coord{  abs(ourBaseCoords.x - static_cast<int>(ceil(cBaseRadius * defendRadiusCoefHightThreatment * cos(70. * M_PI / 180.)))),
                        abs(ourBaseCoords.y - static_cast<int>(ceil(cBaseRadius * defendRadiusCoefHightThreatment * sin(70. * M_PI / 180.)))) }
            }
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