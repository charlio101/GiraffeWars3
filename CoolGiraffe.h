#ifndef _COOlGIRAFFE_H_
#define _COOLGIRAFFE_H_

#include "Giraffe.h"
#include "CoolMoveSet.h"
#include <memory>
#include <array>

constexpr int NUM_MOVES_COOL= 24;
constexpr int NUM_POINTS_COOL = 38;

class CoolGiraffe : public Giraffe {
public:
	CoolGiraffe(Vector2 _Position, MoveSet* _Moves, COLORREF _Colour);
	~CoolGiraffe();

	void Update(std::array<Giraffe*, 4> giraffes, const int num_giraffes, const int i, const int inputs, const int frameNumber, Stage& stage);
	void Draw(HDC hdc, Vector2 Scale, int frameNumber);
private:
	HBRUSH SpitBrush;
	HBRUSH ShineBrush;
	void DrawSelf(HDC hdc, Vector2 Scale, int CurrentFrame, int CurrentAnim);
	void DrawHitbox(HDC hdc, Vector2 Scale, Vector2 Pos, float Rad);
};

#endif