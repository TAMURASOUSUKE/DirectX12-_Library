#include <utility>
#include "UIButtonInputSource.h"

UIButtonInputSource::UIButtonInputSource(InputQuery _pushed, InputQuery _heldQuery, InputQuery _releasedQuery) : 
	pushedQuery{ std::move(_pushed) }, heldQuery{ std::move(_heldQuery) }, releasedQuery{ std::move(_releasedQuery) }
{

}

bool UIButtonInputSource::IsPushed() const
{
	if (!pushedQuery) return false;
	return pushedQuery();
}

bool UIButtonInputSource::IsHeld() const
{
	if (!heldQuery) return false;
	return heldQuery();
}
bool UIButtonInputSource::IsReleased() const
{
	if (!releasedQuery) return false;
	return releasedQuery();
}

bool UIButtonInputSource::IsValid() const
{
	// 問い合わせ処理が一つ以上あるか
	return pushedQuery || heldQuery || releasedQuery;
}
