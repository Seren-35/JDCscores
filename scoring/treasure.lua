if not iswinner then
	return gems
end
local best = 0
for _, other in ipairs(players) do
	if other.gems > best then
		best = other.gems
	end
end
return best * 1.5