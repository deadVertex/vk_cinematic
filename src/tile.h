#pragma once

struct Tile
{
    u32 minX;
    u32 minY;
    u32 maxX;
    u32 maxY;
};

inline u32 ComputeTiles(u32 totalWidth, u32 totalHeight, u32 tileWidth,
    u32 tileHeight, Tile *tiles, u32 maxTiles)
{
    u32 tileCountY = (u32)Ceil((f32)totalHeight / (f32)tileHeight);
    u32 tileCountX = (u32)Ceil((f32)totalWidth / (f32)tileWidth);

    for (u32 tileY = 0; tileY < tileCountY; ++tileY)
    {
        for (u32 tileX = 0; tileX < tileCountX; ++tileX)
        {
            u32 index = tileX + tileY * tileCountX;
            if (index >= maxTiles)
            {
                break;
            }

            u32 x = tileX * tileWidth;
            u32 y = tileY * tileHeight;

            Tile tile;
            tile.minX = tileX * tileWidth;
            tile.minY = tileY * tileHeight;
            tile.maxX = MinU32(tile.minX + tileWidth, totalWidth);
            tile.maxY = MinU32(tile.minY + tileHeight, totalHeight);

            tiles[index] = tile;
        }
    }

    u32 totalTileCount = MinU32(tileCountY * tileCountX, maxTiles);
    return totalTileCount;
}
