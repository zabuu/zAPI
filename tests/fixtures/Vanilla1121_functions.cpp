// Reference functions from ERitzman/UnitXP_SP3, commit 36fca988241840f53720376d787b1f8685956568.
// Retained only for offline parity tests; not compiled into zAPI.dll.

bool CWorld_Intersect(const C3Vector* p1, const C3Vector* p2, C3Vector* intersectPoint, float* distance, uint32_t intersectFlag) {
    // The common knowledge of flag is 0x100171 or 0x100111:
    // *- 0x100171 would cause game crash in Turtle WoW Hateforge Quarry.
    // *- 0x100111 works well.

    // Internet says distance needed to be initialized to 1.0f
    *distance = 1.0f;
    *intersectPoint = {};

    // I witnessed 1 crash without distance guard.
    const float distanceGuard = 150.0f;
    float checkDistance = UnitXP_distanceBetween(*p1, *p2);
    if (checkDistance > distanceGuard) {
        *distance = 0.5f;
        intersectPoint->x = (p2->x + p1->x) / 2;
        intersectPoint->y = (p2->y + p1->y) / 2;
        intersectPoint->z = (p2->z + p1->z) / 2;
        return true;
    }
    if (checkDistance <= std::numeric_limits<float>::epsilon()) {
        return false;
    }

    std::string perfName = "CWorld_Intersect";
    perfSetSlotName(0, perfName);
    perfMarkStart(0);
    bool result = p_CWorld_Intersect(p1, p2, 0, intersectPoint, distance, intersectFlag);
    perfMarkEnd(0);

    return result;
}

bool vanilla1121_unitInLineOfSight(uint32_t unit0, uint32_t unit1) {
    if (unit0 == unit1) {
        return true;
    }

    if (vanilla1121_unitCollisionBoxHeight(unit0) > vanilla1121_unitCollisionBoxHeight(unit1)) {
        // We keep unit 1 is taller
        uint32_t temp = unit0;
        unit0 = unit1;
        unit1 = temp;
    }

    C3Vector pos0 = vanilla1121_unitPosition(unit0);
    C3Vector pos1 = vanilla1121_unitPosition(unit1);

    C3Vector intersectPoint = {};
    float distance = 1.0f;

    // First test: unit 0 try to look at unit 1 at the same height
    // Not a typo
    pos0.z += vanilla1121_unitCollisionBoxHeight(unit0);
    pos1.z += vanilla1121_unitCollisionBoxHeight(unit0);

    bool result = CWorld_Intersect(&pos0, &pos1, &intersectPoint, &distance);

    if (result) {
        if (distance <= 1 && distance >= 0) {
            // 1st test: intersect between points, loss sight
            if (vanilla1121_unitCollisionBoxHeight(unit0) == vanilla1121_unitCollisionBoxHeight(unit1)) {
                return false;
            }

            // Second test: unit 0 try to look up at unit 1
            intersectPoint = {};
            distance = 1.0f;
            pos0 = vanilla1121_unitPosition(unit0);
            pos1 = vanilla1121_unitPosition(unit1);
            pos0.z += vanilla1121_unitCollisionBoxHeight(unit0);
            pos1.z += vanilla1121_unitCollisionBoxHeight(unit1);

            result = CWorld_Intersect(&pos0, &pos1, &intersectPoint, &distance);
            if (result) {
                if (distance <= 1 && distance >= 0) {
                    // 2nd test: intersect between points, loss sight
                    return false;
                }
                else {
                    // 2nd test: intersect after points, still in sight
                    return true;
                }
            }
            else {
                // 2nd test: no intersect, in sight
                return true;
            }
        }
        else {
            // 1st test: intersect after points, still in sight
            return true;
        }
    }
    else {
        // 1st test: no intersect, in sight
        return true;
    }
}
