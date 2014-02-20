// Copyright 2010 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
#include "Game.h"
#include "Render.h"
#include <intrin.h>

extern bool g_bRenderTrees;

Game::Game( void ) : m_fCoverageThreshold( 0.0f ),
                     m_fCoverage( 0.0f ),
                     m_nNumInactiveTiles( 0 ) {}

Game::~Game( void ) {}

float RandFloat( void )
{
    static const float fRecipRandMax = 1.0f / RAND_MAX;
    return ( rand() * fRecipRandMax ) - 0.5f;
}

void Game::Initialize( void )
{
    m_fCoverage = 0.0f;
    m_fCoverageThreshold = 0.9f;
    m_nNumInactiveTiles = 0;
    m_nNumActiveTiles = 0;
    m_nNumActiveFactories = 0;

    // Initialize the tiles
    ZeroMemory( m_pTiles, sizeof( m_pTiles ) );
    for( unsigned int x = 0; x < gs_nWorldSize; ++x )
    {
        for( unsigned int y = 0; y < gs_nWorldSize; ++y )
        {
            unsigned int nIndex = x * gs_nWorldSize + y;
            m_pTiles[nIndex].fX = ( x * gs_fTileSize ) + gs_fTileSize / 2; // Offset by half the tile size so 
            m_pTiles[nIndex].fY = ( y * gs_fTileSize ) + gs_fTileSize / 2; // the position is in the middle 
        }
    }

    // Init the unit manager
    m_UnitManager.Initialize( this );

    // Now reset self
    Reset();

    InitializeCriticalSection( &m_CriticalSection );
}

void Game::Update( float fTime,
                   float fElapsedTime )
{
    m_fElapsedTime = fElapsedTime;
    m_fTime = fTime;

    m_UnitManager.Update( fElapsedTime );

    // Reset on full coverage
    if( m_fCoverage > m_fCoverageThreshold )
        Reset();
}

void Game::Reset( void )
{
    // Stop the unit manager
    m_UnitManager.StopWork();

    // Zero the inactive tiles array
    ZeroMemory( m_pInactiveTiles, sizeof( m_pInactiveTiles ) );
    m_nNumInactiveTiles = 0;
    m_nNumActiveFactories = 0;
    m_nNumActiveTrees = 0;
    m_nNumActiveTiles = 0;
    m_nNumInactiveTrees = 0;
    m_fCoverage = 0.0f;

    // Reset the tiles
    for( unsigned int x = 0; x < gs_nWorldSize; ++x )
    {
        for( unsigned int y = 0; y < gs_nWorldSize; ++y )
        {
            unsigned int nIndex = x * gs_nWorldSize + y;
            m_pTiles[nIndex].bActive = false;
            m_pTiles[nIndex].fTimer = gs_fTileTime;
            m_pTiles[nIndex].bFactory = false;
            m_pTiles[nIndex].nTree = -1;
            m_pInactiveTiles[m_nNumInactiveTiles++] = nIndex;

            if( !( rand() % gs_nTreeGranularity ) && m_nNumActiveTrees < gs_nTreeCount )
            {
                m_pTiles[nIndex].nTree = m_nNumActiveTrees;

                float fScale = 1.0f + ( RandFloat() * 0.2f );
                m_pActiveTreeMatrices[m_nNumActiveTrees++] = XMMatrixScaling( fScale, fScale, fScale ) *
                    XMMatrixRotationY( RandFloat() * XM_2PI ) *
                    XMMatrixTranslation( m_pTiles[nIndex].fX + RandFloat() * gs_fTileSize, 0.0f,
                                         m_pTiles[nIndex].fY + RandFloat() * gs_fTileSize );
            }
        }
    }

    // Now randomize the "out of bounds" trees
    int r = ( int )( gs_nWorldSize * -0.2f );
    r = gs_nWorldSize + ( int )( gs_nWorldSize * 0.2f );
    for( int x = ( int )( gs_nWorldSize * -0.2f ); x < ( int )gs_nWorldSize + ( int )( gs_nWorldSize * 0.2f ); ++x )
    {
        for( int y = ( int )( gs_nWorldSize * -0.2f ); y < ( int )gs_nWorldSize + ( int )( gs_nWorldSize * 0.2f );
             ++y )
        {
            if( x >= 0 && x < gs_nWorldSize && y >= 0 && y < gs_nWorldSize )
                continue;

            if( !( rand() % gs_nTreeGranularity ) && m_nNumInactiveTrees < gs_nTreeCount )
            {
                float fScale = 1.0f + ( RandFloat() * 0.2f );
                m_pInactiveTreeMatrices[m_nNumInactiveTrees++] = XMMatrixScaling( fScale, fScale, fScale ) *
                    XMMatrixRotationY( RandFloat() * XM_2PI ) *
                    XMMatrixTranslation( ( ( x * gs_fTileSize ) + gs_fTileSize / 2 ) + RandFloat() * gs_fTileSize,
                                         0.0f,
                                         ( ( y * gs_fTileSize ) + gs_fTileSize / 2 ) + RandFloat() * gs_fTileSize );
            }
        }
    }

    // Reset the factories
    // Place the minerals
    for( int i = 0; i < gs_nMaxFactories; ++i )
    {
        int x = rand() % gs_nWorldSize;
        int y = rand() % gs_nWorldSize;
        int nIndex = x * gs_nWorldSize + y;
        m_pTiles[nIndex].bFactory = true;

        m_pFactories[i] = nIndex;
    }

    // Reset the unit manager
    m_UnitManager.Reset();
}

void Game::Render( void )
{
    // Factories
    Render::DrawInstanced( m_pFactoryMatrices, FACTORY_MESH, m_nNumActiveFactories, false );

    // Trees
    if( g_bRenderTrees )
    {
        Render::DrawInstanced( m_pActiveTreeMatrices, TREE_MESH, m_nNumActiveTrees );
        Render::DrawInstanced( m_pInactiveTreeMatrices, TREE_MESH, m_nNumInactiveTrees );
    }

    // Units
    Render::DrawInstanced( m_UnitManager.GetTransforms(),
                           UNIT_MESH,
                           m_UnitManager.GetNumUnits() );

    // Terrain
    Render::DrawTerrain( m_pTileMatrices, m_nNumActiveTiles );

    // Sky
    Render::DrawSky();

}

unsigned int Game::GetInactiveTile( void )
{
    int nIndex = rand() % m_nNumInactiveTiles;
    return m_pInactiveTiles[nIndex];
}

void Game::SetTileActive( unsigned int nTile )
{
    assert( nTile >= 0 && nTile < gs_nWorldSizeSq );

    // Make sure its not already active
    if( m_pTiles[ nTile ].bActive )
    {
        return;
    }

    // Activate the tile
    m_pTiles[ nTile ].bActive = true;
    long nIndex = _InterlockedIncrement( &m_nNumActiveTiles );
    // Increment our coverage
    m_fCoverage += gs_fCoveragePerTile;

    // Using a critial section to make sure that threads
    //   aren't modifying m_nNumInactiveTiles while
    //   other threads are reading it
    EnterCriticalSection( &m_CriticalSection );
    // Remove the tile from the inactive tiles array
    for( int i = 0; i < m_nNumInactiveTiles; ++i )
    {
        if( m_pInactiveTiles[i] == nTile )
        {
            long nReplace = _InterlockedDecrement( &m_nNumInactiveTiles );
            m_pInactiveTiles[i] = m_pInactiveTiles[nReplace + 1];
        }
    }
    LeaveCriticalSection( &m_CriticalSection );

    // Render the tile
    if( m_pTiles[ nTile ].bFactory )
    { // With either a factory
        m_pFactoryMatrices[ m_nNumActiveFactories++ ] = XMMatrixTranslation( m_pTiles[nTile].fX, 0.0f,
                                                                             m_pTiles[nTile].fY );
    }
    else
    { // ...or a slab of cement
        m_pTileMatrices[ nIndex - 1 ] = XMMatrixTranslation( m_pTiles[nTile].fX, 0.0f, m_pTiles[nTile].fY );
    }

    // Stop rendering the tree
    if( m_pTiles[nTile].nTree != -1 )
    {
        m_pActiveTreeMatrices[ m_pTiles[nTile].nTree ] = m_pActiveTreeMatrices[ --m_nNumActiveTrees ];
        m_pTiles[nTile].nTree = -1;
    }
}

// Returns true once the tile is paved
bool Game::PaveTile( unsigned int nTile )
{
    m_pTiles[nTile].fTimer -= m_fElapsedTime;

    int nTree = m_pTiles[nTile].nTree;

    if( nTree != -1 )
    {
        m_pActiveTreeMatrices[nTree]._42 -= m_fElapsedTime;
    }

    if( m_pTiles[nTile].fTimer <= 0.0f )
    {
        SetTileActive( nTile );
        return true;
    }

    return false;
}