# One Day in the Life of Ivan Denisovich: The Video Game

## Rules

- Walk left and right, A to jump (say 2 tiles high?)
- Your position always comes to rest at tile intervals.
- Without shovel, B to pick up the thing below you or drop what you're carrying.
- With shovel, B to dig one tile of earth or deposit the tile's worth you're carrying.
- Stand in front of shovel and press Down to pick up, or Down while holding it to drop it there.
- World begins flat.
- World loops horizontally (say 5 screens wide?).
- World extends vertically far enough that nobody should reach the edge.
- Begin with a Stalin statue resting at ground level.
- Whenever the Stalin statue is not the tallest thing, a warning sounds and guards will shoot you.
- ~Game over if you get shot.~ Hit points, say 5?
- Each round is timed, say 2 minutes?
- End of round, your score is the lowest depth times the highest elevation times hit points.
- - ie if you don't dig at all, or don't build at all, or get killed, no points.
- - Also no points if the statue is not the highest thing, we don't give points to Enemies Of The State.
- Radioactive waste! It appears midway through the game and you must bury it in the deepest hole.

## TODO

- [x] Bricks and barrels
- [x] World map thumbnail
- [x] Move tattle rendering to global game (draw in front of thumbnail)
- [x] Clock
- [x] Scorekeeping
- [x] Truck deliveries
- [x] Guards
- [x] Statue
- [ ] Sound effects
- [x] Main menu
- [x] Death animation, don't just hop to the menu immediately.
- [ ] Tiny menu image
- [x] Fairy appears when you're trapped and creates new dirt
- [ ] Get permission from the Solzhenitsyn estate: https://www.solzhenitsyncenter.org/who-we-are
- [x] Watch the 1970 movie. There's a movie!
- - https://www.youtube.com/watch?v=YqG1uwhTX2o
- - ...it doesn't do justice to the book (an example i intend to follow...)
- [ ] Fall to ground when you die, currently he'll stop midair
- [ ] Guard can sometimes shoot thru walls (only leftward?) Bullet looks higher than usual when this happens
- [ ] Cheat camera up a little, now that we have UI chrome on top ...think it thru, maybe not (consider standing on a lone peak)
- [x] Truck, eliminate the passive tile so we can dump things on the hood.
- [x] Are there some finer-grade criteria we can score against? Movement? HP? ...yes and yes
- [ ] Persist high score
