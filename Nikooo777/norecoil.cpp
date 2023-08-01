//
// Created by Niko on 7/22/2021.
//
#include "norecoil.h"
#include "common.h"
#include "CCSPlayer.h"
#include "Helper.h"


void noRecoil(CUserCmd *userCmd) {
	static Vector3 oldPunch = {0, 0, 0};
	auto helper = Helper::getInstance();
	auto shotsFired = ((CCSPlayer *) helper->GetLocalPlayer())->m_iShotsFired;
	auto punchAngle = helper->GetLocalPlayer()->m_Local.m_vecPunchAngle;

	Vector3 tempAngle = {0, 0, 0};
	if (shotsFired > 0) {
		tempAngle.x = (userCmd->viewangles.x + oldPunch.x) - (punchAngle.x * 2);
		tempAngle.y = (userCmd->viewangles.y + oldPunch.y) - (punchAngle.y * 2);
		tempAngle.NormalizeAngles();
		tempAngle.ClampAngles();
		oldPunch.x = punchAngle.x * 2;
		oldPunch.y = punchAngle.y * 2;
		userCmd->viewangles = tempAngle;
	} else {
		oldPunch = {0, 0, 0}; // reset old punch
	}
}
