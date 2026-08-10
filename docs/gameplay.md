# Gameplay port notes

Research on the original mechanics: `RodLegacyResearch/docs/gameplay.md`.

## Challenge retry ("he changed his mind")

The original runs the bet dialog up to twice, with an independent acceptance
roll each attempt - see the research doc for the loop and why it matters.

Ported in `src/challenge.cpp`.

`Reject()` (the player's own "Forget it") still ends the encounter on the spot
with no retry, which matches the original's immediate break on that choice. A
separate `OpponentRejectsBet()` handles the roll-failure path: it counts the
attempt in `betAttempts` and sets `insultFromOpponent`, and once the insult
bubble clears, `Draw()` calls `ShowRaceTypeDialog()` again for the second
attempt as long as `betAttempts < kMaxBetAttempts` (2), otherwise sets
`wantsCarExit`.

Fixed alongside it: `ShowDragBetDialog()` used to call `Accept()`
unconditionally with no roll at all, skipping the acceptance table entirely for
every drag race. It now rolls through `OpponentAcceptsBet` the same way
`ShowRoadBetDialog()` already did, using `BetType::NoMoney` / `SmallMoney` /
`LargeMoney` for kicks / $25 / $50. Without that fix the retry loop could never
trigger on a drag offer at all.

`OpponentAcceptsBet` in `src/bet_acceptance.cpp` is a simplified port of the
original's per-bet accept/reject roll.

**Not ported:** the original rerolls the insult line so it never repeats the
previous one. We pick a random insult on each rejection independent of the
last, so a repeat is possible. Cosmetic, left alone deliberately.
