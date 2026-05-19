/**
 * Minimap pixel coordinates → game world coordinates.
 * From valorant-api.com coordinate conversion formula.
 *
 *   game_Y = (norm_X - xScalarToAdd) / xMultiplier
 *   game_X = (norm_Y - yScalarToAdd) / yMultiplier
 *
 * where norm_X = pixelX / imageWidth (normalized 0-1)
 */
export function minimapToGame(pixelX, pixelY, imgW, imgH, mapParams) {
  const normX = pixelX / imgW;
  const normY = pixelY / imgH;

  const gameY = (normX - mapParams.xScalarToAdd) / mapParams.xMultiplier;
  const gameX = (normY - mapParams.yScalarToAdd) / mapParams.yMultiplier;

  return { x: gameX, y: gameY, z: 0 };
}

/**
 * Reverse: game coordinates → minimap pixel position.
 */
export function gameToMinimap(gameX, gameY, imgW, imgH, mapParams) {
  const normY = gameX * mapParams.yMultiplier + mapParams.yScalarToAdd;
  const normX = gameY * mapParams.xMultiplier + mapParams.xScalarToAdd;

  return {
    x: normX * imgW,
    y: normY * imgH,
  };
}

export function calcDistance(pos1, pos2) {
  const dx = pos1.x - pos2.x;
  const dy = pos1.y - pos2.y;
  return Math.sqrt(dx * dx + dy * dy);
}
