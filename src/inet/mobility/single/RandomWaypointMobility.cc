//
// Copyright (C) 2005 Georg Lutz, Institut fuer Telematik, University of Karlsruhe
// Copyright (C) 2005 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
//

#include "inet/mobility/single/RandomWaypointMobility.h"

namespace inet {

Define_Module(RandomWaypointMobility);

RandomWaypointMobility::RandomWaypointMobility()
{
    nextMoveIsWait = false;
}

void RandomWaypointMobility::Visitor::visit(const cObject *object) const
{
    if (!isObstacleFound_)
        isObstacleFound_ = obstacles->isObstacle(check_and_cast<const physicalenvironment::IPhysicalObject *>(object), transmissionPosition, receptionPosition);
}

RandomWaypointMobility::Visitor::Visitor(RandomWaypointMobility *mod, const Coord& endPosition, const Coord& initialPosition):
        obstacles(mod),
        transmissionPosition(endPosition),
        receptionPosition(initialPosition)
{

}

bool RandomWaypointMobility::isObstacle(const physicalenvironment::IPhysicalObject *object, const Coord& transmissionPosition, const Coord& receptionPosition) const
{
    const ShapeBase *shape = object->getShape();
    const Coord& position = object->getPosition();
    const Quaternion& orientation = object->getOrientation();
    RotationMatrix rotation(orientation.toEulerAngles());
    const LineSegment lineSegment(rotation.rotateVectorInverse(transmissionPosition - position), rotation.rotateVectorInverse(receptionPosition - position));
    Coord intersection1, intersection2, normal1, normal2;
    bool hasIntersections = shape->computeIntersection(lineSegment, intersection1, intersection2, normal1, normal2);
    bool isObstacle = hasIntersections && intersection1 != intersection2;
    return isObstacle;
}

void RandomWaypointMobility::initialize(int stage)
{
    LineSegmentsMobilityBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        waitTimeParameter = &par("waitTime");
        hasWaitTime = waitTimeParameter->isExpression() || waitTimeParameter->doubleValue() != 0;
        speedParameter = &par("speed");
        stationary = !speedParameter->isExpression() && speedParameter->doubleValue() == 0;
        physicalEnvironment = findModuleFromPar<physicalenvironment::IPhysicalEnvironment>(par("physicalEnvironmentModule"), this->getSimulation()->getSystemModule());
    }
}


void RandomWaypointMobility::setTargetPosition()
{
    if (nextMoveIsWait) {
        simtime_t waitTime = waitTimeParameter->doubleValue();
        nextChange = simTime() + waitTime;
        nextMoveIsWait = false;
    }
    else {
        bool obs = false;
        do {
            targetPosition = getRandomPosition();
            if (physicalEnvironment) {
                Visitor visitor(this, targetPosition, this->getCurrentPosition());
                // check obstacles, if it cross an obstacle, select other
                physicalEnvironment->visitObjects(&visitor, LineSegment(targetPosition, this->getCurrentPosition()));
                obs = visitor.isObstacleFound();
            }
        } while(obs);
        double speed = speedParameter->doubleValue();
        double distance = lastPosition.distance(targetPosition);
        simtime_t travelTime = distance / speed;
        nextChange = simTime() + travelTime;
        nextMoveIsWait = hasWaitTime;
    }
}



void RandomWaypointMobility::move()
{
    LineSegmentsMobilityBase::move();
    raiseErrorIfOutside();
}

double RandomWaypointMobility::getMaxSpeed() const
{
    return speedParameter->isExpression() ? NaN : speedParameter->doubleValue();
}

} // namespace inet

