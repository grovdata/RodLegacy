#pragma once

#include "global.h"
#include "car.h"

class Game {

    private:
        int m_bankroll;


	public:
		Game() : m_bankroll(750)  {

		}

        int GetBankroll() {
            return m_bankroll;
        }
};