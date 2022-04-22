#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cmath>
#include <algorithm>

using namespace std;

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
    int id, type, shieldLife, isControlled, healt, vx, vy, nearBase, threatFor;
    Coord coords;
    double importance;
    void read(const Coord& baseCoords) {
        cin >> id >> type >> coords.x >> coords.y >> shieldLife >> isControlled >> healt >> vx >> vy >> nearBase >> threatFor; cin.ignore();
        importance = cMaxDist - dist(baseCoords, coords);
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

    void readBaseCoords() {
        cin >> baseCoords.x >> baseCoords.y; cin.ignore();
    }

    void read() {
        monsters.clear(); our.clear(); enemies.clear(); dangerousMonsters.clear();
        cin >> entitiesNb; cin.ignore();
        for (int entityId = 0; entityId < entitiesNb; entityId++) {
            tempEntity.read(baseCoords);
            switch (tempEntity.type) {
            case Monster: monsters.push_back(tempEntity); break;
            case Our:     our.push_back(tempEntity); break;
            case Enemy:   enemies.push_back(tempEntity); break;
            default: break;
            }
        }
    }

    void updateDangerousMonsters() {
        int maxSimulationDepth = 6;
        int baseDangerousRadius = cBaseRadius / 2;
        for (int simulationDepth = 0; simulationDepth < maxSimulationDepth; simulationDepth++) {
            // simulate monster pos for simulationDepth iteration
            for (const Entity& monster : monsters) {
                Coord simulatedCoord{ monster.coords.x + simulationDepth * monster.vx, monster.coords.y + simulationDepth * monster.vy };
                double distToBase = dist(simulatedCoord, baseCoords);
                if (distToBase < baseDangerousRadius) {
                    dangerousMonsters.push_back(monster);
                    dangerousMonsters.back().importance = cMaxDist - distToBase + (maxSimulationDepth - simulationDepth - 1) * 2500.;
                }
            }
        }
        sort(dangerousMonsters.begin(), dangerousMonsters.end(), entityCompare);
    }

    Coord getWaitCoords() {
        return { abs(baseCoords.x - cBaseRadius/2), abs(baseCoords.y - cBaseRadius / 2) };
    }

    vector<Coord> getBestTargets() {
        vector<Coord> bestTargets;

        /*
        criteria 1
        sort(monsters.begin(), monsters.end(), entityCompare);
        for (size_t targetIdx = 0; targetIdx < min(3, static_cast<int>(monsters.size())); targetIdx++)
        {
            bestTargets.push_back(monsters[targetIdx].coords);
        }
        */

        // criteria 5
        for (size_t targetIdx = 0; targetIdx < min(3, static_cast<int>(dangerousMonsters.size())); targetIdx++)
        {
            bestTargets.push_back(dangerousMonsters[targetIdx].coords);
        }

        return bestTargets;
    }
} entities;

int main() {
    entities.readBaseCoords();
    int heroesNb; cin >> heroesNb; cin.ignore();
    const Coord waitCoords = entities.getWaitCoords();

    while (1) {
        bases.read();
        entities.read();

        entities.updateDangerousMonsters();

        vector<Coord> bestTargets = entities.getBestTargets();
        for (size_t targetIdx = 0; targetIdx < 3; targetIdx++) {
            if (targetIdx < bestTargets.size())
            {
                cout << "MOVE " << bestTargets[targetIdx].x << " " << bestTargets[targetIdx].y << endl;
            }
            else
            {
                cout << "WAIT" << endl;
            }
        }
    }
}