if place == 0 then
	return 0
end
local score = 0
for _, other in ipairs(players) do
	if other.place > place then
		score = score + 1 / (other.place - 1)
	end
end
return math.ceil(score * 100)