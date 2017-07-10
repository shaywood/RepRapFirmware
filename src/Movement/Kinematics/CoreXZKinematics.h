/*
 * CoreXZKinematics.h
 *
 *  Created on: 6 May 2017
 *      Author: David
 */

#ifndef SRC_MOVEMENT_KINEMATICS_COREXZKINEMATICS_H_
#define SRC_MOVEMENT_KINEMATICS_COREXZKINEMATICS_H_

#include "CoreBaseKinematics.h"

class CoreXZKinematics : public CoreBaseKinematics
{
public:
	CoreXZKinematics();

	// Overridden base class functions. See Kinematics.h for descriptions.
	const char *GetName(bool forStatusReport) const override;
	bool CartesianToMotorSteps(const float machinePos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, int32_t motorPos[]) const override;
	void MotorStepsToCartesian(const int32_t motorPos[], const float stepsPerMm[], size_t numVisibleAxes, size_t numTotalAxes, float machinePos[]) const override;
	uint32_t AxesToHomeBeforeProbing() const override { return (1u << X_AXIS) | (1u << Y_AXIS) | (1u << Z_AXIS); }
	bool DriveIsShared(size_t drive) const override;
	bool SupportsAutoCalibration() const override { return false; }
};

#endif /* SRC_MOVEMENT_KINEMATICS_COREXZKINEMATICS_H_ */
