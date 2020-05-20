#include "Giraffe.h"
#include <math.h>
#include "GiraffeFactory.h"
#include <fstream>
#include <string>


void NormAttacks(Giraffe& g)
{
	if ((g.State & STATE_HEAVY) && (g.State & STATE_UP) && g.AnimFrame == 17) {
		++g.LastAttackID;
	}
	else if ((g.State & STATE_JUMPING) && (g.State & STATE_WEAK) && (g.State & STATE_BACK) && g.AnimFrame == 7) {
		g.Facing.x *= -1;
	}
}

//int Giraffe::AttackID;
//int Giraffe::counter;

Giraffe::Giraffe()
{
	////Movement
	//Position = Vec2(0, 0);
	//Velocity = Vec2(0, 0);
	//MaxGroundSpeed = 3;
	//MaxAirSpeed = Vec2(2, 5);
	//RunAccel = 0.5f;
	//AirAccel = 0.2f;
	//Gravity = 0.2f;
	//Facing = true;

	////Jumping
	//JumpSpeed = 5;
	//HasDoubleJump = false;
	//DashSpeed = 4;
	//HasAirDash = true;

	////Collision
	//Fullbody = { Vec2(0,0), 20 };
	//numHitboxes = 0;
	//numIncoming = 0;
	//PrevHitQueue = ArrayQueue();

	////State
	//State = 0;
	//JumpDelay = 0;
	//MaxJumpDelay = 4;
	//AttackDelay = 0;
	//MaxShieldDelay = 5;
	//Moves = GiraffeFactory::CreateMoveSet(0);

	////Misc
	//Stocks = 3;
	//Knockback = 1;
	//Mass = 1;
	//DrawSelf = GiraffeFactory::GetDrawFunc(0);
}


Giraffe::Giraffe(Vec2 position, const MoveSet* moves)
{
	//Movement
	Position = position;
	Velocity = Vec2(0, 0);
	MaxGroundSpeed = 0.4;
	MaxAirSpeed = Vec2(0.3, 0.7);
	RunAccel = 0.1f;
	AirAccel = 0.03f;
	Gravity = 0.02f;
	Facing = { 1, 1 };

	//Jumping
	JumpSpeed = 0.5;
	HasDoubleJump = false;
	DashSpeed = 0.4;
	HasAirDash = true;

	//Collision
	Fullbody = { Vec2(0,0), 2.5 };
	StageCollider = { Vec2(0,0.7), 1 };
	Hitboxes = nullptr;
	Hurtboxes = nullptr;
	numHitboxes = 0;
	numIncoming = 0;
	PrevHitQueue = ArrayQueue();
	LastAttackID = 0;

	//State
	State = 0;
	JumpDelay = 0;
	MaxJumpDelay = 4;
	AttackDelay = 0;
	MaxShieldDelay = 5;
	Moves = moves;

	/*std::ofstream file("NormalizedHitboxes.txt");
	Vec2 normForce;
	for (int i = 8; i < 20; ++i) {
		file << "{";
		for (int f = 0; f < Moves->Hitboxes[i].size(); ++f) {
			file << "{";
			for (int v = 0; v < Moves->Hitboxes[i][f].size(); ++v) {
				normForce = Moves->Hitboxes[i][f][v].Force;
				normForce = normForce.Normalise();
				file << "HitCollider(" << "{" << Moves->Hitboxes[i][f][v].Position.x << "f," << Moves->Hitboxes[i][f][v].Position.y << "f}," << Moves->Hitboxes[i][f][v].Radius << "f,{" << normForce.x << "f," << normForce.y << "f}," << Moves->Hitboxes[i][f][v].Damage << "f),";
			}
			file << "},";
		}
		file << "};\n";
	}
	file.close();*/



	//Misc
	Stocks = 3;
	Knockback = 0;
	Mass = 100;
	DrawSelf = GiraffeFactory::GetDrawFunc(0);
	CharAttacks = &NormAttacks;

	//Animation
	AnimFrame = 0;
	ShieldBrush = CreateHatchBrush(HS_BDIAGONAL, RGB(0, 255, 127));
	//ShieldBrush = CreateSolidBrush(RGB(0, 0, 0));
}

void Giraffe::ParseInputs(const int inputs, const int frameNumber)
{
	if (!(State & (STATE_WEAK | STATE_HEAVY | STATE_SHIELDING | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_DROPSHIELD | STATE_HITSTUN | STATE_ATTACKSTUN))) {
		if (inputs & INPUT_LEFT) {
			if (State & STATE_JUMPING) {
				if (Velocity.x > -MaxAirSpeed.x) {
					Velocity.x -= AirAccel;
				}
			}
			else if (Velocity.x > -MaxGroundSpeed) {
				Velocity.x -= RunAccel;
				State |= STATE_RUNNING;
				Facing = { -1, 1 };
			}
		}
		else if (inputs & INPUT_RIGHT) {
			if (State & STATE_JUMPING) {
				if(Velocity.x < MaxAirSpeed.x){
					Velocity.x += AirAccel;
				}
			}
			else if (Velocity.x < MaxGroundSpeed) {
				Velocity.x += RunAccel;
				State |= STATE_RUNNING;
				Facing = { 1, 1 };
			}
		}
		else {
			State &= ~STATE_RUNNING;
		}

		if (inputs & INPUT_DOWN && State & STATE_JUMPING && !(inputs & (INPUT_WEAK | INPUT_HEAVY))) {
			State |= STATE_FASTFALL;
		}
	}



	if (inputs & INPUT_WEAK && !(State & (STATE_WEAK | STATE_HEAVY | STATE_SHIELDING | STATE_DROPSHIELD | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_HITSTUN | STATE_WAVEDASH | STATE_ATTACKSTUN))) {
		State |= STATE_WEAK;
		++LastAttackID;
		AnimFrame = -1;
		if ((inputs & INPUT_RIGHT && Facing.x == 1) || (inputs & INPUT_LEFT && Facing.x == -1)) {
			State |= STATE_FORWARD;
			AttackNum = 9;
		}
		else if (inputs & INPUT_UP) {
			State |= STATE_UP;
			AttackNum = 10;
		}
		else if (inputs & INPUT_DOWN) {
			State |= STATE_DOWN;
			AttackNum = 11;
		}
		else {//Neutral
			AttackNum = 8;
		}
		AttackDelay = frameNumber + Moves->SkelPoints[AttackNum].size();
		if (State & STATE_JUMPING) {
			if ((inputs & INPUT_RIGHT && Facing.x == -1) || (inputs & INPUT_LEFT && Facing.x == 1)) {
				//Bair
				State |= STATE_BACK;
				AttackNum = 19;
			}
			else {
				AttackNum += 7;
			}
		}
	}
	if (inputs & INPUT_HEAVY && !(State & (STATE_WEAK | STATE_HEAVY | STATE_SHIELDING | STATE_DROPSHIELD | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_HITSTUN | STATE_WAVEDASH | STATE_ATTACKSTUN))) {
		if ((inputs & INPUT_RIGHT && Facing.x == 1) || (inputs & INPUT_LEFT && Facing.x == -1)) {
			//Fsmash
			State |= STATE_FORWARD;
			AttackNum = 12;
			AttackDelay = frameNumber + Moves->SkelPoints[12].size();
			State |= STATE_HEAVY;
			++LastAttackID;
			AnimFrame = -1;
		}
		else if (inputs & INPUT_UP) {
			//Upsmash
			State |= STATE_UP;
			AttackNum = 13;
			AttackDelay = frameNumber + Moves->SkelPoints[13].size();
			State |= STATE_HEAVY;
			++LastAttackID;
			AnimFrame = -1;
		}
		else if (inputs & INPUT_DOWN)
		{
			//Dsmash
			State |= STATE_DOWN;
			AttackNum = 14;
			AttackDelay = frameNumber + Moves->SkelPoints[14].size();
			State |= STATE_HEAVY;
			++LastAttackID;
			AnimFrame = -1;
		}
	}


	if (inputs & INPUT_JUMP && !(State & (STATE_WEAK | STATE_HEAVY | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_HITSTUN | STATE_SHIELDSTUN | STATE_WAVEDASH | STATE_ATTACKSTUN))) {
		if (!(State & STATE_JUMPING)) {
			State |= STATE_JUMPSQUAT;
			JumpDelay = frameNumber + MaxJumpDelay;
			AnimFrame= -1;
			State &= ~(STATE_UP | STATE_BACK | STATE_DOWN | STATE_FORWARD | STATE_WEAK | STATE_SHIELDING | STATE_DROPSHIELD | STATE_RUNNING);
		}
		else if (HasDoubleJump) {
			HasDoubleJump = false;
			Velocity.y = -JumpSpeed;
			State &= ~STATE_FASTFALL;
		}
	}
	else if (!(inputs & INPUT_JUMP) && (State & STATE_JUMPSQUAT)) {
		State |= STATE_SHORTHOP;
	}


	if (inputs & INPUT_SHIELD) {
		if (inputs & INPUT_DOWN && State & STATE_JUMPSQUAT && HasAirDash) {
			HasAirDash = false;
			State &= ~(STATE_JUMPSQUAT | STATE_SHORTHOP);
			State |= STATE_WAVEDASH;
			JumpDelay = frameNumber + 2 * MaxJumpDelay;

			Velocity.y += DashSpeed;
			if (inputs & INPUT_LEFT) {
				Velocity.x -= DashSpeed;
			}
			else if (inputs & INPUT_RIGHT) {
				Velocity.x += DashSpeed;
			}
		}
		else if (State & STATE_JUMPING && !(State & (STATE_WEAK | STATE_HEAVY | STATE_HITSTUN)) && HasAirDash) {
			HasAirDash = false;
			State &= ~STATE_FASTFALL;
			if (inputs & INPUT_LEFT) {
				Velocity.x -= DashSpeed;
			}
			else if (inputs & INPUT_RIGHT) {
				Velocity.x += DashSpeed;
			}
			if (inputs & INPUT_UP) {
				Velocity.y -= DashSpeed;
			}
			else if (inputs & INPUT_DOWN) {
				Velocity.y += DashSpeed;
			}
		}
		else if (!(State & (STATE_WEAK | STATE_HEAVY | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_JUMPING | STATE_HITSTUN | STATE_SHIELDSTUN | STATE_WAVEDASH | STATE_ATTACKSTUN))) {
			State &= ~(STATE_DROPSHIELD | STATE_WAVEDASH | STATE_RUNNING);
			State |= STATE_SHIELDING;
			Velocity.x = 0;
		}
	}
	else if (State & STATE_SHIELDING) {
		State &= ~STATE_SHIELDING;
		State |= STATE_DROPSHIELD;
		AttackDelay += MaxShieldDelay;
	}

}

void Giraffe::Update(Giraffe giraffes[], const int num_giraffes, const int i, const int frameNumber)
{
	//counter++;
	++AnimFrame;
	//Transition States Based On Frame Number
	//Jumping
	if (State & STATE_JUMPSQUAT && frameNumber >= JumpDelay) {
		if (State & STATE_SHORTHOP) {
			Velocity.y = -0.5 * JumpSpeed;
		}
		else {
			Velocity.y = -JumpSpeed;
		}
		State &= ~(STATE_JUMPSQUAT | STATE_SHORTHOP);
		State |= STATE_JUMPING | STATE_DOUBLEJUMPWAIT;
		JumpDelay = frameNumber + MaxJumpDelay * 2;
	}
	else if (State & STATE_JUMPLAND && frameNumber >= JumpDelay) {
		State &= ~STATE_JUMPLAND;
	}
	else if (State & STATE_DOUBLEJUMPWAIT && frameNumber >= JumpDelay) {
		HasDoubleJump = true;
		State &= ~STATE_DOUBLEJUMPWAIT;
	}
	else if (State & STATE_WAVEDASH && frameNumber >= JumpDelay) {
		State &= ~STATE_WAVEDASH;
	}
	//Attacking
	if (State & (STATE_WEAK | STATE_HEAVY) && frameNumber >= AttackDelay) {
		State &= ~(STATE_UP | STATE_BACK | STATE_DOWN | STATE_FORWARD | STATE_WEAK | STATE_HEAVY);
		numHitboxes = 0;
	}
	//Hitstun
	else if (State & STATE_HITSTUN && frameNumber >= AttackDelay) {
		State &= ~STATE_HITSTUN;
	}
	else if (State & STATE_ATTACKSTUN && frameNumber >= AttackDelay) {
		State &= ~STATE_ATTACKSTUN;
	}
	else if (State & STATE_SHIELDSTUN && frameNumber >= AttackDelay) {
		State &= ~STATE_SHIELDSTUN;
	}
	//Dropping shield
	else if (State & STATE_DROPSHIELD && frameNumber >= AttackDelay) {
		State &= ~STATE_DROPSHIELD;
	}


	if (State & (STATE_WEAK | STATE_HEAVY)) {
		Hitboxes = &(Moves->Hitboxes[AttackNum][AnimFrame]);
		Hurtboxes = &(Moves->Hurtboxes[AttackNum][AnimFrame]);
		numHitboxes = (*Hitboxes).size();
	}
	else {
		Hitboxes = nullptr;

		if (State & STATE_HITSTUN) {
			Hurtboxes = &(Moves->Hurtboxes[6][AnimFrame % 9]);
		}
		else if (State & STATE_RUNNING) {
			Hurtboxes = &(Moves->Hurtboxes[1][AnimFrame % 2]);
		}
		else if (State & STATE_JUMPING) {
			Hurtboxes = &(Moves->Hurtboxes[2][0]);
		}
		else if (State & STATE_JUMPSQUAT) {
			Hurtboxes = &(Moves->Hurtboxes[3][AnimFrame]);
		}
		else if (State & STATE_JUMPLAND) {
			Hurtboxes = &(Moves->Hurtboxes[4][AnimFrame]);
		}
		else { //Idle
			Hurtboxes = (&Moves->Hurtboxes[0][0]);
		}
	}


	//Adding Gravity
	if (State & STATE_JUMPING) {
		if (Velocity.y < MaxAirSpeed.y) {
			Velocity.y += Gravity;
		}
		if (State & STATE_FASTFALL) {
			Velocity.y += 3 * Gravity;
		}
	}
	
	//Adding Friction/Air resitance
	if (!(State & STATE_JUMPING)) {
		Velocity *= 0.9f;
	}
	else {
		if (fabs(Velocity.y) > MaxAirSpeed.y) {
			Velocity.y *= 0.8f;
		}
		if (fabs(Velocity.x) > MaxAirSpeed.x) {
			Velocity.x *= 0.95f;
		}
	}

	//Update character specific state
	CharAttacks(*this);


	//Apply hits to other giraffes
	if (!(Hitboxes == nullptr)) {
		for (int j = 0; j < num_giraffes; ++j) {
			if (j != i) {
				for (int h = 0; h < numHitboxes; ++h) {
					if (Intersect(Position, (*Hitboxes)[h], Facing, giraffes[j].Position, giraffes[j].Fullbody, giraffes[j].Facing)) {
						for (int k = 0; k < 6; ++k) {
							if (Intersect(Position, (*Hitboxes)[h], Facing, giraffes[j].Position, (*giraffes[j].Hurtboxes)[k], giraffes[j].Facing)) {
								giraffes[j].AddHit((*Hitboxes)[h], (*giraffes[j].Hurtboxes)[k].DamageMultiplier, LastAttackID, Facing);
								break;
							}
						}
						break;
					}
				}
			}
		}
	}
	//Recieve incoming hits
	if (State & STATE_SHIELDING) {
		for (int j = 0; j < numIncoming; ++j) {
			if (!PrevHitQueue.Contains(IncomingHits[j].ID)) {
				State |= STATE_SHIELDSTUN;
				AttackDelay = max(AttackDelay, frameNumber + IncomingHits[j].hit.Damage * 50);
				PrevHitQueue.Push(IncomingHits[j].ID);
			}
		}
	}
	else {
		for (int j = 0; j < numIncoming; ++j) {
			if (!PrevHitQueue.Contains(IncomingHits[j].ID)) {
				Knockback += IncomingHits[j].hit.Damage;
				//Velocity += (Knockback / Mass) * IncomingHits[j].hit.Force;
				if (IncomingHits[j].hit.Fixed) {
					Velocity += IncomingHits[j].hit.Knockback * IncomingHits[j].hit.Force;
				}
				else {
					Velocity += ((((Knockback / 10 + (Knockback * IncomingHits[j].hit.Damage / 20)) * (200 / (Mass + 100)) * 1.4 + 0.18) * IncomingHits[j].hit.Scale) + IncomingHits[j].hit.Knockback) * IncomingHits[j].hit.Force;
				}
				State &= ~(STATE_UP | STATE_BACK | STATE_DOWN | STATE_FORWARD | STATE_WEAK | STATE_HEAVY | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_SHORTHOP | STATE_ATTACKSTUN);
				State |= STATE_HITSTUN;
				AttackDelay = max(AttackDelay, frameNumber + IncomingHits[j].hit.Damage * 100);
				PrevHitQueue.Push(IncomingHits[j].ID);
			}
		}
	}
	numIncoming = 0;
}

void Giraffe::Move(Stage stage, int frameNumber)
{
	Position += Velocity;
	//Correct intersection with the stage
	int direction;
	float offset;
	if (stage.Intersects(Position, StageCollider, direction, offset)) {
		switch (direction) {
		case 0:
			Position.y += offset;
			Velocity.y = 0;
			if (State & STATE_JUMPING) {
				if (State & STATE_WEAK) {
					State |= STATE_ATTACKSTUN;
					AttackDelay = Moves->LandingLag[AttackNum - 15];
				}


				State &= ~(STATE_UP | STATE_BACK | STATE_DOWN | STATE_FORWARD | STATE_WEAK | STATE_HEAVY | STATE_JUMPING | STATE_FASTFALL);
				State |= STATE_JUMPLAND;
				HasAirDash = true;
				HasDoubleJump = false;
				JumpDelay = frameNumber + MaxJumpDelay / 2;
				AnimFrame = 0;
			}
			break;
		case 1:
			Position.y += offset;
			Velocity.y = 0;
			break;
		case 2:
			Position.x += offset;
			Velocity.x = 0;
			break;
		case 3:
			Position.x += offset;
			Velocity.x = 0;
			break;
		}
	}
	else if (!(State & STATE_JUMPING)) {
		State &= ~(STATE_UP | STATE_BACK | STATE_DOWN | STATE_FORWARD | STATE_WEAK | STATE_HEAVY | STATE_SHIELDING | STATE_DROPSHIELD | STATE_SHIELDSTUN | STATE_JUMPSQUAT | STATE_SHORTHOP | STATE_JUMPLAND | STATE_WAVEDASH | STATE_ATTACKSTUN);
		State |= STATE_JUMPING | STATE_DOUBLEJUMPWAIT;
		JumpDelay = frameNumber + MaxJumpDelay * 2;
	}


	//Make giraffes loop around like pac-man
	//Remove this later
	if (Position.y > 50) {
		Position.y = 0;
	}
	else if (Position.y < 0) {
		Position.y = 50;
	}
	if (Position.x > 60) {
		Position.x = 0;
	}
	else if (Position.x < 0) {
		Position.x = 60;
	}

	//This is the last stateful part of the update loop
}

void Giraffe::AddHit(HitCollider hit, const float multiplier, int ID, Vec2 Facing)
{
	IncomingHits[numIncoming].hit = hit;
	IncomingHits[numIncoming].hit.Force *= Facing;
	//IncomingHits[numIncoming].hit.Damage *= multiplier;
	IncomingHits[numIncoming].ID = ID;
	++numIncoming;
}


//The draw function !!!CANNOT!!! have state modification
void Giraffe::Draw(HDC hdc, Vec2 Scale)
{
	//I CAN SEE YOU PUTTING STATE IN THE DRAW FUNCTION
	//THIS IS A STRICTLY NO STATE ZONE

	if (State & (STATE_WEAK | STATE_HEAVY)) {
		DrawSelf(hdc, Moves->SkelPoints[AttackNum][AnimFrame], Position, Facing, Scale);
	}
	else {
		if (State & (STATE_HITSTUN | STATE_ATTACKSTUN)) {
			DrawSelf(hdc, Moves->SkelPoints[6][AnimFrame % 9], Position, Facing, Scale);
		}
		else if (State & STATE_SHIELDSTUN) {
			SelectObject(hdc, ShieldBrush);
			Ellipse(hdc, (Position.x - 2.5) * Scale.x, (Position.y - 2.5) * Scale.y, (Position.x + 2.5) * Scale.x, (Position.y + 2.5) * Scale.y);
			DrawSelf(hdc, Moves->SkelPoints[6][AnimFrame % 9], Position, Facing, Scale);
		}
		else if (State & STATE_SHIELDING) {
			SelectObject(hdc, ShieldBrush);
			Ellipse(hdc, (Position.x - 2.5) * Scale.x, (Position.y - 2.5) * Scale.y, (Position.x + 2.5) * Scale.x, (Position.y + 2.5) * Scale.y);
			DrawSelf(hdc, Moves->SkelPoints[0][0], Position, Facing, Scale);
		}
		else if (State & STATE_RUNNING) {
			DrawSelf(hdc, Moves->SkelPoints[1][AnimFrame % 2], Position, Facing, Scale);
		}
		else if (State & STATE_JUMPING) {
			DrawSelf(hdc, Moves->SkelPoints[2][0], Position, Facing, Scale);
		}
		else if (State & STATE_JUMPSQUAT) {
			DrawSelf(hdc, Moves->SkelPoints[3][AnimFrame], Position, Facing, Scale);
		}
		else if (State & STATE_JUMPLAND) {
			DrawSelf(hdc, Moves->SkelPoints[4][AnimFrame], Position, Facing, Scale);
		}
		else if (State & STATE_WAVEDASH) {
			DrawSelf(hdc, Moves->SkelPoints[7][0], Position, Facing, Scale);
		}
		else {
			DrawSelf(hdc, Moves->SkelPoints[0][0], Position, Facing, Scale);
		}
	}
	//NO STATE NO STATE NO STATE NO STATE NO STATE NO STATE
	
	/*if (Hitboxes != nullptr) {
		for (int i = 0; i < numHitboxes; ++i) {
			Ellipse(hdc, (Position.x + (*Hitboxes)[i].Position.x * Facing.x - (*Hitboxes)[i].Radius) * Scale.x, (Position.y + (*Hitboxes)[i].Position.y - (*Hitboxes)[i].Radius) * Scale.y, (Position.x + (*Hitboxes)[i].Position.x * Facing.x + (*Hitboxes)[i].Radius) * Scale.x, (Position.y + (*Hitboxes)[i].Position.y + (*Hitboxes)[i].Radius) * Scale.y);
		}
	}

	if (Hurtboxes != nullptr) {
		for (int i = 0; i < 6; ++i) {
			Ellipse(hdc, (Position.x + (*Hurtboxes)[i].Position.x * Facing.x - (*Hurtboxes)[i].Radius) * Scale.x, (Position.y + (*Hurtboxes)[i].Position.y - (*Hurtboxes)[i].Radius) * Scale.y, (Position.x + (*Hurtboxes)[i].Position.x * Facing.x + (*Hurtboxes)[i].Radius) * Scale.x, (Position.y + (*Hurtboxes)[i].Position.y + (*Hurtboxes)[i].Radius) * Scale.y);
		}
	}*/
	//DO NOT PUT STATE IN THE DRAW FUNCTION
	//DONT YOU DO IT

}

