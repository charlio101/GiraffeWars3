#include "CoolGiraffe.h"
#include "CoolProjFuncs.h"
#include "NormProjFuncs.h"//TEMP

CoolGiraffe::CoolGiraffe(Vector2 _Position, MoveSet* _Moves, COLORREF _Colour)
{
	//Movement
	Position = _Position;
	Velocity = Vector2(0, 0);
	MaxGroundSpeed = 0.4f;
	MaxAirSpeed = Vector2(0.3f, 0.7f);
	RunAccel = 0.1f;
	AirAccel = 0.03f;
	Gravity = 0.02f;
	Facing = { 1.0f, 1.0f };

	//Jumping
	JumpSpeed = 0.5f;
	HasDoubleJump = false;
	DashSpeed = 0.4f;
	HasAirDash = true;

	//Collision
	Fullbody = { Vector2(0.0f,0.0f), 2.5f };
	StageCollider = { Vector2(0.0f,0.7f), 1.0f };
	Hitboxes = nullptr;
	Hurtboxes = nullptr;
	numHitboxes = 0;
	numIncoming = 0;
	PrevHitQueue = ArrayQueue<int>();
	LastAttackID = 0;
	Projectiles = ArrayList<Projectile>();
	incomingGrab = false;

	//State
	State = 0;
	SoundMoveState = 0;
	SoundAttackState = 0;
	JumpDelay = 0;
	MaxJumpDelay = 4;
	AttackDelay = 0;
	AttackNum = 0;
	MaxShieldDelay = 5;
	TechDelay = 0;
	Moves = _Moves;

	//Misc
	Stocks = 3;
	Knockback = 0;
	Mass = 100;
	CommandGrabPointer = 0;

	//Animation
	AnimFrame = 0;
	GiraffePen = CreatePen(PS_SOLID, 1, _Colour);
	IntangiblePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
	ShieldBrush = CreateHatchBrush(HS_BDIAGONAL, RGB(0, 255, 127));
	SpitBrush = CreateSolidBrush(RGB(90, 210, 180));
	ShineBrush = CreateSolidBrush(RGB(0, 255, 255));
}

CoolGiraffe::~CoolGiraffe()
{
}

void CoolGiraffe::Update(std::array<Giraffe*, 4> giraffes, const int num_giraffes, const int i, const int inputs, const int frameNumber, Stage& stage)
{
	++AnimFrame;

	//Transition States Based On Frame Number
	if (State & STATE_JUMPSQUAT && frameNumber >= JumpDelay) {
		if (State & STATE_SHORTHOP) {
			Velocity.y = -0.5f * JumpSpeed;
		}
		else {
			Velocity.y = -JumpSpeed;
		}
		State &= ~(STATE_JUMPSQUAT | STATE_SHORTHOP);
		State |= STATE_JUMPING | STATE_DOUBLEJUMPWAIT;
		JumpDelay = frameNumber + MaxJumpDelay * 2;
		SoundMoveState |= SOUND_JUMP;
		SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_JUMP] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_JUMP);
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
	if (State & (STATE_WEAK | STATE_HEAVY | STATE_THROW) && frameNumber >= AttackDelay) {
		State &= ~(STATE_UP | STATE_BACK | STATE_DOWN | STATE_FORWARD | STATE_WEAK | STATE_HEAVY | STATE_THROW | STATE_GETUPATTACK);
		numHitboxes = 0;
	}
	else if (State & STATE_HITSTUN && frameNumber >= AttackDelay) {
		State &= ~STATE_HITSTUN;
		SoundMoveState &= ~SOUND_HITSTUN;
	}
	else if (State & STATE_SHIELDSTUN && frameNumber >= AttackDelay) {
		State &= ~STATE_SHIELDSTUN;
	}
	else if (State & STATE_DROPSHIELD && frameNumber >= AttackDelay) {
		State &= ~STATE_DROPSHIELD;
	}
	else if (State & STATE_ROLLING && frameNumber >= AttackDelay) {
		State &= ~STATE_ROLLING;
	}
	if (State & STATE_TECHATTEMPT && frameNumber >= TechDelay) {
		State &= ~STATE_TECHATTEMPT;
		State |= STATE_TECHLAG;
		TechDelay = frameNumber + 40;
	}
	else if (State & STATE_TECHLAG && frameNumber >= TechDelay) {
		State &= ~STATE_TECHLAG;
	}
	else if (State & STATE_TECHING && frameNumber >= TechDelay) {
		State &= ~STATE_TECHING;
		State &= ~STATE_INTANGIBLE;
	}
	else if (State & STATE_KNOCKDOWNLAG && frameNumber >= TechDelay) {
		State &= ~STATE_KNOCKDOWNLAG;
		State |= STATE_KNOCKDOWN;
	}
	else if (State & (STATE_GRABBING | STATE_GRABBED) && frameNumber >= TechDelay) {
		State &= ~(STATE_GRABBED | STATE_GRABBING);
	}

	for (int i = 0; i < XACT_WAVEBANK_MOVEBANK_ENTRY_COUNT; ++i) {
		if (SoundMoveState & (1 << i) && frameNumber >= SoundMoveDelay[i]) {
			SoundMoveState &= ~(1 << i);
		}
	}
	for (int i = 0; i < XACT_WAVEBANK_ATTACKBANK_ENTRY_COUNT; ++i) {
		if (SoundAttackState & (1 << i) && frameNumber >= SoundAttackDelay[i]) {
			SoundAttackState &= ~(1 << i);
		}
	}


	//Read Inputs
	if (!(State & (STATE_WEAK | STATE_HEAVY | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_DROPSHIELD | STATE_SHIELDSTUN | STATE_HITSTUN | STATE_KNOCKDOWNLAG | STATE_ROLLING | STATE_GRABBED | STATE_THROW))) {
		if (State & STATE_LEDGEHOG) {
			if (inputs & INPUT_DOWN) {
				State &= ~STATE_LEDGEHOG;
				stage.Ledges[LedgeID].Hogged = false;
				State |= STATE_JUMPING | STATE_DOUBLEJUMPWAIT;
				JumpDelay = frameNumber + MaxJumpDelay * 2;
				SoundMoveState &= ~SOUND_HITSTUN;
			}
		}
		else if (State & STATE_GRABBING) {
			if ((inputs & INPUT_RIGHT && Facing.x == 1) || (inputs & INPUT_LEFT && Facing.x == -1)) {
				State &= ~STATE_GRABBING;
				State |= STATE_FORWARD | STATE_THROW;
				AttackNum = 13;
				AttackDelay = frameNumber + Moves->GetMoveLength(AttackNum);
				AnimFrame = 0;
				LastAttackID++;
				SoundAttackState |= SOUND_FTHROW;
				SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_FTHROW] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_FTHROW);
			}
			else if (inputs & INPUT_UP) {
				State &= ~STATE_GRABBING;
				State |= STATE_UP | STATE_THROW;
				AttackNum = 14;
				AttackDelay = frameNumber + Moves->GetMoveLength(AttackNum);
				AnimFrame = 0;
				LastAttackID++;
				SoundAttackState |= SOUND_UPTHROW;
				SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_UPTHROW] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_UPTHROW);
			}
			else if (inputs & INPUT_DOWN) {
				State &= ~STATE_GRABBING;
				State |= STATE_DOWN | STATE_THROW;
				AttackNum = 15;
				AttackDelay = frameNumber + Moves->GetMoveLength(AttackNum);
				AnimFrame = 0;
				LastAttackID++;
				SoundAttackState |= SOUND_DOWNTHROW;
				SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_DOWNTHROW] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_DOWNTHROW);
			}
			else if ((inputs & INPUT_LEFT && Facing.x == 1) || (inputs & INPUT_RIGHT && Facing.x == -1)) {
				State &= ~STATE_GRABBING;
				State |= STATE_BACK | STATE_THROW;
				AttackNum = 16;
				AttackDelay = frameNumber + Moves->GetMoveLength(AttackNum);
				AnimFrame = 0;
				LastAttackID++;
				SoundAttackState |= SOUND_BACKTHROW;
				SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_BACKTHROW] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_BACKTHROW);
			}
		}
		else if (State & STATE_JUMPING) {
			if (inputs & INPUT_LEFT) {
				if (Velocity.x > -MaxAirSpeed.x) {
					Velocity.x -= AirAccel;
				}
			}
			else if (inputs & INPUT_RIGHT) {
				if (Velocity.x < MaxAirSpeed.x) {
					Velocity.x += AirAccel;
				}
			}
			if (inputs & INPUT_DOWN && !(inputs & (INPUT_WEAK | INPUT_HEAVY)) && !(State & STATE_FASTFALL)) {
				State |= STATE_FASTFALL;
				SoundMoveState |= SOUND_FASTFALL;
				SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_FASTFALL] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_FASTFALL);
			}
		}
		else {
			if (inputs & INPUT_LEFT) {
				if (State & (STATE_TECHING | STATE_SHIELDING | STATE_KNOCKDOWN)) {
					State &= ~(STATE_TECHING | STATE_SHIELDING | STATE_KNOCKDOWN | STATE_CROUCH);
					State |= STATE_ROLLING | STATE_INTANGIBLE;
					AttackDelay = frameNumber + 20;
					AnimFrame = 0;
					Facing = { -1, 1 };
					SoundMoveState |= SOUND_ROLL;
					SoundMoveState &= ~SOUND_SHIELD;
					SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_ROLL] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_ROLL);
				}
				else if (Velocity.x > -MaxGroundSpeed) {
					Velocity.x -= RunAccel;
					State &= ~STATE_CROUCH;
					State |= STATE_RUNNING;
					Facing = { -1, 1 };
					SoundMoveState |= SOUND_RUN;
					SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_RUN] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_RUN);
				}
			}
			else if (inputs & INPUT_RIGHT) {
				if (State & (STATE_TECHING | STATE_SHIELDING | STATE_KNOCKDOWN)) {
					State &= ~(STATE_TECHING | STATE_SHIELDING | STATE_KNOCKDOWN | STATE_CROUCH);
					State |= STATE_ROLLING | STATE_INTANGIBLE;
					AttackDelay = frameNumber + 20;
					AnimFrame = 0;
					Facing = { 1, 1 };
					SoundMoveState |= SOUND_ROLL;
					SoundMoveState &= ~SOUND_SHIELD;
					SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_ROLL] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_ROLL);
				}
				else if (Velocity.x < MaxGroundSpeed) {
					Velocity.x += RunAccel;
					State &= ~STATE_CROUCH;
					State |= STATE_RUNNING;
					Facing = { 1, 1 };
					SoundMoveState |= SOUND_RUN;
					SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_RUN] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_RUN);
				}
			}
			else {
				State &= ~STATE_RUNNING;
				SoundMoveState &= ~SOUND_RUN;
			}

			if (inputs & INPUT_DOWN && !(inputs & (INPUT_WEAK | INPUT_HEAVY)) && !(State & (STATE_CROUCH | STATE_ROLLING))) {
				State |= STATE_CROUCH;
				SoundMoveState |= SOUND_CROUCH;
				SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_CROUCH] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_CROUCH);
			}
			else if (!(inputs & INPUT_DOWN) && State & STATE_CROUCH) {
				State &= ~STATE_CROUCH;
			}
		}
	}
	if (inputs & INPUT_WEAK && !(State & (STATE_WEAK | STATE_HEAVY | STATE_SHIELDSTUN | STATE_DROPSHIELD | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_HITSTUN | STATE_WAVEDASH | STATE_KNOCKDOWNLAG | STATE_TECHING | STATE_ROLLING | STATE_GRABBING | STATE_GRABBED | STATE_THROW))) {
		State |= STATE_WEAK;
		State &= ~STATE_CROUCH;
		if (State & STATE_KNOCKDOWN) {
			State &= ~STATE_KNOCKDOWN;
			State |= STATE_GETUPATTACK;
			AttackNum = 17;
		}
		else if (State & STATE_LEDGEHOG) {
			State &= ~STATE_LEDGEHOG;
			stage.Ledges[LedgeID].Hogged = false;
			SoundMoveState &= ~SOUND_HITSTUN;
			State |= STATE_GETUPATTACK;
			AttackNum = 17;
			Position += Vector2(2.5f, -2.5f) * Facing;
		}
		else if ((inputs & INPUT_SHIELD || State & STATE_SHIELDING) && !(State & (STATE_JUMPING))) {
			State &= ~STATE_SHIELDING;
			State |= STATE_HEAVY;//Code grabs as weak and heavy
			AttackNum = 12;
		}
		else if ((inputs & INPUT_RIGHT && Facing.x == 1) || (inputs & INPUT_LEFT && Facing.x == -1)) {
			State |= STATE_FORWARD;
			AttackNum = 19;
		}
		else if (inputs & INPUT_UP) {
			State |= STATE_UP;
			AttackNum = 20;
		}
		else if (inputs & INPUT_DOWN) {
			State |= STATE_DOWN;
			AttackNum = 21;
		}
		else {//Neutral
			AttackNum = 18;
		}
		if (State & STATE_JUMPING) {
			if ((inputs & INPUT_RIGHT && Facing.x == -1) || (inputs & INPUT_LEFT && Facing.x == 1)) {
				//Bair
				State |= STATE_BACK;
				AttackNum = 29;
			}
			else {
				AttackNum += 7;
			}
		}
		AttackDelay = frameNumber + Moves->GetMoveLength(AttackNum);
		SoundAttackState |= (1 << (AttackNum - 12));
		SoundAttackDelay[AttackNum - 12] = frameNumber + Moves->GetAttackSoundLength(AttackNum - 12);
		++LastAttackID;
		AnimFrame = 0;
	}
	if (inputs & INPUT_HEAVY && !(State & (STATE_WEAK | STATE_HEAVY | STATE_SHIELDING | STATE_DROPSHIELD | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_HITSTUN | STATE_WAVEDASH | STATE_LEDGEHOG | STATE_KNOCKDOWN | STATE_KNOCKDOWNLAG | STATE_TECHING | STATE_ROLLING | STATE_GRABBING | STATE_GRABBED | STATE_THROW))) {
		State &= ~STATE_CROUCH;
		State |= STATE_HEAVY;
		if ((inputs & INPUT_RIGHT && Facing.x == 1) || (inputs & INPUT_LEFT && Facing.x == -1)) {
			//Fsmash
			State |= STATE_FORWARD;
			AttackNum = 22;
		}
		else if (inputs & INPUT_UP) {
			//Upsmash
			State |= STATE_UP;
			AttackNum = 23;
		}
		else if (inputs & INPUT_DOWN)
		{
			//Dsmash
			State |= STATE_DOWN;
			AttackNum = 24;
		}
		else
		{
			//Neutral B
			AttackNum = 30;
		}
		if (State & STATE_JUMPING) {
			if (State & (STATE_UP | STATE_DOWN | STATE_FORWARD)) {
				AttackNum += 9;
			}
		}
		AttackDelay = frameNumber + Moves->GetMoveLength(AttackNum);
		SoundAttackState |= (1 << (AttackNum - 12));
		SoundAttackDelay[AttackNum - 12] = frameNumber + Moves->GetAttackSoundLength(AttackNum - 12);
		++LastAttackID;
		AnimFrame = 0;
	}

	if (inputs & INPUT_JUMP && !(State & (STATE_WEAK | STATE_HEAVY | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_HITSTUN | STATE_SHIELDSTUN | STATE_WAVEDASH | STATE_KNOCKDOWNLAG | STATE_TECHING | STATE_ROLLING | STATE_GRABBING | STATE_GRABBED | STATE_THROW))) {
		if (State & STATE_LEDGEHOG) {
			State &= ~STATE_LEDGEHOG;
			stage.Ledges[LedgeID].Hogged = false;
			State |= STATE_JUMPING | STATE_DOUBLEJUMPWAIT;
			JumpDelay = frameNumber + MaxJumpDelay;
			Velocity.y = -JumpSpeed;
			AnimFrame = 0;
			SoundMoveState |= SOUND_JUMP;
			SoundMoveState &= ~SOUND_HITSTUN;
			SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_JUMP] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_JUMP);
		}
		else if (!(State & STATE_JUMPING)) {
			State |= STATE_JUMPSQUAT;
			JumpDelay = frameNumber + MaxJumpDelay;
			AnimFrame = 0;
			State &= ~(STATE_SHIELDING | STATE_DROPSHIELD | STATE_RUNNING | STATE_CROUCH | STATE_KNOCKDOWN);
			SoundMoveState &= ~(SOUND_SHIELD | SOUND_RUN);
		}
		else if (HasDoubleJump) {
			HasDoubleJump = false;
			Velocity.y = -JumpSpeed;
			State &= ~STATE_FASTFALL;
			SoundMoveState |= SOUND_DOUBLEJUMP;
			SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_DOUBLEJUMP] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_DOUBLEJUMP);
		}
	}
	else if (!(inputs & INPUT_JUMP) && (State & STATE_JUMPSQUAT)) {
		State |= STATE_SHORTHOP;
	}
	else if (inputs & INPUT_JUMP && State & STATE_HEAVY && State & STATE_DOWN && State & STATE_JUMPING && HasDoubleJump) {
		State &= ~(STATE_HEAVY | STATE_DOWN);
		HasDoubleJump = false;
		Velocity.y = -JumpSpeed;
		State &= ~STATE_FASTFALL;
		SoundAttackState &= ~SOUND_DOWNB;
		SoundMoveState |= SOUND_DOUBLEJUMP;
		SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_DOUBLEJUMP] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_DOUBLEJUMP);
	}

	if (inputs & INPUT_SHIELD) {
		if ((State & STATE_JUMPING) && !(State & (STATE_TECHLAG | STATE_TECHATTEMPT | STATE_GRABBING | STATE_GRABBED))) {
			State |= STATE_TECHATTEMPT;
			TechDelay = frameNumber + 20;
		}

		if (inputs & INPUT_DOWN && State & STATE_JUMPSQUAT && HasAirDash) {
			HasAirDash = false;
			State &= ~(STATE_JUMPSQUAT | STATE_SHORTHOP);
			State |= STATE_WAVEDASH;
			JumpDelay = frameNumber + 2 * MaxJumpDelay;
			SoundMoveState |= SOUND_WAVEDASH;
			SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_WAVEDASH] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_WAVEDASH);

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
			SoundMoveState |= SOUND_AIRDASH;
			SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_AIRDASH] = frameNumber + Moves->GetMoveSoundLength(XACT_WAVEBANK_MOVEBANK_AIRDASH);
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
		else if (!(State & (STATE_WEAK | STATE_HEAVY | STATE_JUMPSQUAT | STATE_JUMPLAND | STATE_JUMPING | STATE_HITSTUN | STATE_SHIELDSTUN | STATE_WAVEDASH | STATE_LEDGEHOG | STATE_KNOCKDOWN | STATE_KNOCKDOWNLAG | STATE_ROLLING | STATE_GRABBING | STATE_GRABBED | STATE_THROW))) {
			State &= ~(STATE_DROPSHIELD | STATE_WAVEDASH | STATE_RUNNING);
			State |= STATE_SHIELDING;
			Velocity.x = 0;
			SoundMoveState |= SOUND_SHIELD;
			SoundMoveDelay[XACT_WAVEBANK_MOVEBANK_SHIELD] = 1000000000;
		}
	}
	else if (State & STATE_SHIELDING) {
		State &= ~STATE_SHIELDING;
		State |= STATE_DROPSHIELD;
		AttackDelay = frameNumber + MaxShieldDelay;
		SoundMoveState &= ~SOUND_SHIELD;
	}

	if (State & STATE_ROLLING) {
		Velocity.x = MaxGroundSpeed * Facing.x;
		if (AnimFrame == 10) {
			State &= ~STATE_INTANGIBLE;
		}
	}

	//Grab
	if (State & STATE_WEAK && State & STATE_HEAVY && AnimFrame == 7) {
		Collider grabCol = { {Position.x + 1.885000f * Facing.x, Position.y - 0.650000f}, 0.855862f };
		for (int j = 0; j < num_giraffes; ++j) {
			if (j != i) {
				if (giraffes[j]->GrabHit(grabCol, Facing, frameNumber)) {
					State &= ~(STATE_WEAK | STATE_HEAVY | STATE_RUNNING);
					State |= STATE_GRABBING;
					TechDelay = frameNumber + 30;
					//GrabPointer = j;
					break;
				}
			}
		}
	}
	//B-Reverse
	if ((State & STATE_HEAVY) && (State & STATE_JUMPING) && AnimFrame == 1 && (((inputs & INPUT_LEFT) && Facing.x == 1) || ((inputs & INPUT_RIGHT) && Facing.x == -1))) {
		Facing.x *= -1;
	}

	//Character Move-Specific Updates
	if ((State & STATE_JUMPING) && (State & STATE_HEAVY) && (State & STATE_FORWARD) && AnimFrame == 7) {
		Collider grabCol = { {Position.x + 1.885000f * Facing.x, Position.y - 0.650000f}, 0.855862f };
		for (int j = 0; j < num_giraffes; ++j) {
			if (j != i) {
				if (giraffes[j]->GrabHit(grabCol, Facing, frameNumber)) {
					State &= ~(STATE_WEAK | STATE_HEAVY | STATE_RUNNING);
					State |= STATE_GRABBING;
					TechDelay = frameNumber + 30;
					giraffes[j]->Velocity = Velocity;
					giraffes[j]->State |= STATE_JUMPING;
					CommandGrabPointer = j;
					break;
				}
			}
		}
	}


	if ((State & STATE_HEAVY) && (State & STATE_UP)) {
		//Fire neck
		if (State & STATE_JUMPING) {
			if (AnimFrame == 10) {
				Projectiles.Append(Projectile(Position + Vector2(0.2f, -1.2f), { Facing.x * 0.65f, -0.65f }, 0.3f, { 0.0f, 0.0f }, 0.1f, 0.1f, 1.0f, true, LastAttackID, AttackDelay - 10, NormProjFuncs::NeckGrabOnHit, NormProjFuncs::NeckGrabUpdate, NormProjFuncs::NeckGrabDraw, GiraffePen, SpitBrush));
			}
		}
		//Main hit of upsmash
		else if (AnimFrame == 17) {
			++LastAttackID;
		}
	}
	//Fire spit
	else if ((State & STATE_HEAVY) && !(State & (STATE_WEAK | STATE_UP | STATE_FORWARD | STATE_BACK | STATE_DOWN)) && AnimFrame == 13) {
		Projectiles.Append(Projectile(Position + Vector2(0.2f, -1.2f), { Facing.x * 0.5f, 0.0f }, 0.3f, { Facing.x, 0.0f }, 0.1f, 0.1f, 1.0f, true, LastAttackID, frameNumber + 100, NormProjFuncs::SpitOnHit, NormProjFuncs::SpitUpdate, NormProjFuncs::SpitDraw, GiraffePen, SpitBrush));
	}
	//Reverse direction in bair
	else if ((State & STATE_JUMPING) && (State & STATE_WEAK) && (State & STATE_BACK) && AnimFrame == 7) {
		Facing.x *= -1;
	}
	//Spin like a maniac in get-up attack
	else if ((State & STATE_GETUPATTACK) && (7 <= AnimFrame) && (AnimFrame <= 10)) {
		Facing.x *= -1;
	}


	//Update State
	if (State & (STATE_WEAK | STATE_HEAVY | STATE_THROW)) {
		Hitboxes = Moves->GetHitboxes(AttackNum, AnimFrame);
		Hurtboxes = Moves->GetHurtboxes(AttackNum, AnimFrame);
		numHitboxes = (int)(*Hitboxes).size();
	}
	else {
		Hitboxes = nullptr;
		numHitboxes = 0;
		if (State & STATE_HITSTUN) {
			Hurtboxes = Moves->GetHurtboxes(6, AnimFrame % 16);
		}
		else if (State & STATE_RUNNING) {
			Hurtboxes = Moves->GetHurtboxes(1, AnimFrame % 2);
		}
		else if (State & STATE_JUMPING) {
			Hurtboxes = Moves->GetHurtboxes(2, 0);
		}
		else if (State & STATE_JUMPSQUAT) {
			Hurtboxes = Moves->GetHurtboxes(3, AnimFrame);
		}
		else if (State & STATE_JUMPLAND) {
			Hurtboxes = Moves->GetHurtboxes(4, AnimFrame);
		}
		else if (State & (STATE_WAVEDASH | STATE_CROUCH)) {
			Hurtboxes = Moves->GetHurtboxes(7, 0);
		}
		else if (State & STATE_ROLLING) {
			Hurtboxes = Moves->GetHurtboxes(8, 0);
		}
		else if (State & STATE_LEDGEHOG) {
			Hurtboxes = Moves->GetHurtboxes(6, AnimFrame % 16);
		}
		else if (State & STATE_GRABBING) {
			Hurtboxes = Moves->GetHurtboxes(12, 7);
		}
		else { //Idle
			Hurtboxes = Moves->GetHurtboxes(0, 0);
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
	for (int p = Projectiles.Size() - 1; p >= 0; --p) {
		if (Projectiles[p].Update(Projectiles[p], *this, frameNumber)) {
			Projectiles.Remove(p);
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




	//Apply hits to other giraffes
	for (int p = Projectiles.Size() - 1; p >= 0; --p) {
		for (int j = 0; j < num_giraffes; ++j) {
			if (j != i) {
				if ((*giraffes[j]).ProjectileHit(Projectiles[p])) {
					Projectiles[p].OnHit(Projectiles[p], *this, giraffes[j], frameNumber);
					Projectiles.Remove(p);
					SoundAttackState |= SOUND_WEAK;
					SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_WEAK] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_WEAK);
					break;
				}
			}
		}
	}


	if (!(Hitboxes == nullptr)) {
		for (int j = 0; j < num_giraffes; ++j) {
			if (j != i) {
				for (int h = 0; h < numHitboxes; ++h) {
					if ((*giraffes[j]).AddHit((*Hitboxes)[h], LastAttackID, Facing, Position)) {
						if ((*Hitboxes)[h].Damage > 1) {
							SoundAttackState |= SOUND_HEAVY;
							SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_HEAVY] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_HEAVY);
						}
						else if ((*Hitboxes)[h].Damage > 0.5f) {
							SoundAttackState |= SOUND_MEDIUM;
							SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_MEDIUM] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_MEDIUM);
						}
						else {
							SoundAttackState |= SOUND_WEAK;
							SoundAttackDelay[XACT_WAVEBANK_ATTACKBANK_WEAK] = frameNumber + Moves->GetAttackSoundLength(XACT_WAVEBANK_ATTACKBANK_WEAK);
						}
					}
				}
			}
		}
	}

}

void CoolGiraffe::Draw(HDC hdc, Vector2 Scale, int frameNumber)
{
	for (int i = 0; i < Projectiles.Size(); ++i) {
		Projectiles[i].Draw(Projectiles[i], *this, hdc, Scale, frameNumber);
	}

	int CurrentAnim = 0;
	int CurrentFrame = 0;


	if (State & STATE_INTANGIBLE) {
		SelectObject(hdc, IntangiblePen);
	}
	else {
		SelectObject(hdc, GiraffePen);
	}


	if ((State & STATE_HEAVY) && (State & STATE_JUMPING)) {
		if (State & STATE_UP) {
			if (AnimFrame >= 10 && AnimFrame <= 30) {
				POINT points[NUM_POINTS];
				for (int i = 0; i < NUM_POINTS; ++i) {
					points[i] = Giraffe::VecToPoint(Position + Facing * (*Moves->GetSkelPoints(CurrentAnim, CurrentFrame))[i], Scale);
				}
				Polyline(hdc, points, 27);
				Polyline(hdc, &points[36], 2);
			}
			else {
				DrawSelf(hdc, Scale, AnimFrame, AttackNum);
			}
		}
		else if (State & STATE_DOWN) {
			DrawSelf(hdc, Scale, AnimFrame, AttackNum);
			SelectObject(hdc, ShineBrush);
			POINT points[12];
			float r = 2.5f * cosf(AnimFrame / 15.0f * 3.1415f);
			float r1 = 0.5f * r;
			float r2 = 0.866025f * r;
			points[0] = Giraffe::VecToPoint(Position + Vector2(r, 0), Scale);
			points[1] = Giraffe::VecToPoint(Position + Vector2(r1, r2), Scale);
			points[2] = Giraffe::VecToPoint(Position + Vector2(-r1, r2), Scale);
			points[3] = Giraffe::VecToPoint(Position + Vector2(-r, 0), Scale);
			points[4] = Giraffe::VecToPoint(Position + Vector2(-r1, -r2), Scale);
			points[5] = Giraffe::VecToPoint(Position + Vector2(r1, -r2), Scale);

			r *= 0.6f;
			r1 *= 0.6f;
			r2 *= 0.6f;
			points[6] = Giraffe::VecToPoint(Position + Vector2(r, 0), Scale);
			points[7] = Giraffe::VecToPoint(Position + Vector2(r1, r2), Scale);
			points[8] = Giraffe::VecToPoint(Position + Vector2(-r1, r2), Scale);
			points[9] = Giraffe::VecToPoint(Position + Vector2(-r, 0), Scale);
			points[10] = Giraffe::VecToPoint(Position + Vector2(-r1, -r2), Scale);
			points[11] = Giraffe::VecToPoint(Position + Vector2(r1, -r2), Scale);

			Polygon(hdc, points, 12);
		}
		else {
			DrawSelf(hdc, Scale, AnimFrame, AttackNum);
		}
		return;
	}

	if (State & (STATE_WEAK | STATE_HEAVY | STATE_THROW)) {
		CurrentAnim = AttackNum;
		CurrentFrame = AnimFrame;
		/*for (int i = 0; i < numHitboxes; ++i) {
			DrawHitbox(hdc, Scale, (*Hitboxes)[i].Position, (*Hitboxes)[i].Radius);
		}*/
	}
	else {
		if (State & STATE_HITSTUN) {
			CurrentAnim = 6;
			CurrentFrame = AnimFrame % 16;
		}
		else if (State & STATE_SHIELDSTUN) {
			SelectObject(hdc, ShieldBrush);
			DrawHitbox(hdc, Scale, { 0,0 }, 2.5f);
			//Ellipse(hdc, (Position.x - 2.5f) * Scale.x, (Position.y - 2.5f) * Scale.y, (Position.x + 2.5f) * Scale.x, (Position.y + 2.5f) * Scale.y);
			CurrentAnim = 6;
			CurrentFrame = AnimFrame % 16;
		}
		else if (State & STATE_SHIELDING) {
			SelectObject(hdc, ShieldBrush);
			DrawHitbox(hdc, Scale, { 0,0 }, 2.5f);
			//Ellipse(hdc, (Position.x - 2.5f) * Scale.x, (Position.y - 2.5f) * Scale.y, (Position.x + 2.5f) * Scale.x, (Position.y + 2.5f) * Scale.y);
			CurrentAnim = 0;
			CurrentFrame = 0;
		}
		else if (State & STATE_RUNNING) {
			CurrentAnim = 1;
			CurrentFrame = AnimFrame % 36;
		}
		else if (State & STATE_JUMPING) {
			CurrentAnim = 2;
			CurrentFrame = 0;
		}
		else if (State & STATE_JUMPSQUAT) {
			CurrentAnim = 3;
			CurrentFrame = AnimFrame;
		}
		else if (State & STATE_JUMPLAND) {
			CurrentAnim = 4;
			CurrentFrame = AnimFrame;
		}
		else if (State & STATE_KNOCKDOWNLAG) {
			CurrentAnim = 5;
			CurrentFrame = AnimFrame;
		}
		else if (State & STATE_KNOCKDOWN) {
			CurrentAnim = 5;
			CurrentFrame = 30;
		}
		else if (State & STATE_LEDGEHOG) {
			CurrentAnim = 9;
			CurrentFrame = 0;
		}
		else if (State & (STATE_WAVEDASH | STATE_CROUCH)) {
			CurrentAnim = 7;
			CurrentFrame = 0;
		}
		else if (State & STATE_ROLLING) {
			CurrentAnim = 8;
			CurrentFrame = AnimFrame;
		}
		else if (State & STATE_GRABBED) {
			CurrentAnim = 6;
			CurrentFrame = AnimFrame % 16;
		}
		else if (State & STATE_GRABBING) {
			CurrentAnim = 12;
			CurrentFrame = 7;
		}
		else {
			CurrentAnim = 0;
			CurrentFrame = AnimFrame % 45;
		}
	}
	DrawSelf(hdc, Scale, CurrentFrame, CurrentAnim);
}

void CoolGiraffe::DrawSelf(HDC hdc, Vector2 Scale, int CurrentFrame, int CurrentAnim)
{
	std::vector<POINT> points;
	std::vector<Vector2> vPoints = (*Moves->GetSkelPoints(CurrentAnim, CurrentFrame));

	for (int i = 0; i < vPoints.size(); ++i) {
		points.push_back(Giraffe::VecToPoint(Position + Facing * vPoints[i], Scale));
	}

	if (points.size() > 40) {
		Polyline(hdc, &points[0], 30);
		PolyBezier(hdc, &points[29], 4);
		Polyline(hdc, &points[32], 5);
		PolyBezier(hdc, &points[36], 4);
		Polyline(hdc, &points[39], 2);
		Polyline(hdc, &points[41], 19);

		Vector2 dir = vPoints[36] - vPoints[35];
		dir.Normalize();
		Vector2 perp = { -dir.y, dir.x };

		for (int i = 0; i < 20; ++i) {
			points[i] = VecToPoint(Position + Facing * (vPoints[36] + i / 20.0f * dir + 0.1f * sinf((AnimFrame + i) / (0.5f * 3.14159f)) * perp), Scale);
		}
		Polyline(hdc, &points[0], 20);
	}
	else {
		Polyline(hdc, &points[0], 27);
		PolyBezier(hdc, &points[26], 4);
		Polyline(hdc, &points[29], 5);
		PolyBezier(hdc, &points[33], 4);
		Polyline(hdc, &points[36], 2);
	}
}

void CoolGiraffe::DrawHitbox(HDC hdc, Vector2 Scale, Vector2 Pos, float Rad)
{
	Ellipse(hdc, Scale.x * (Position.x + Facing.x * Pos.x - Rad), Scale.y * (Position.y + Facing.y * Pos.y - Rad), Scale.x * (Position.x + Facing.x * Pos.x + Rad), Scale.y * (Position.y + Facing.y * Pos.y + Rad));
}
