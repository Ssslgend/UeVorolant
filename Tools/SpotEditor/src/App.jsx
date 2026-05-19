import React, { useState, useEffect, useRef } from 'react';
import { fetchMaps } from './services/api';
import { calcDistance } from './services/coordUtils';
import Minimap from './components/Minimap';
import SpotList from './components/SpotList';
import ExportPanel from './components/ExportPanel';

export default function App() {
  const [maps, setMaps] = useState([]);
  const [selectedMapUuid, setSelectedMapUuid] = useState('');
  const [spots, setSpots] = useState([]);
  const [placingMode, setPlacingMode] = useState(null);
  const [pendingSpot, setPendingSpot] = useState(null);
  const minimapRef = useRef(null);

  const selectedMap = maps.find(m => m.uuid === selectedMapUuid);

  useEffect(() => {
    fetchMaps().then(setMaps);
  }, []);

  const handleAddSpot = ({ pixelX, pixelY, gamePos, imgW, imgH }) => {
    if (placingMode === 'player') {
      setPendingSpot({
        playerPixel: { x: pixelX, y: pixelY, imgW, imgH },
        player: gamePos,
      });
    } else if (placingMode === 'enemy' && pendingSpot) {
      const dist = calcDistance(pendingSpot.player, gamePos);
      const newSpot = {
        name: `Spot ${spots.length + 1}`,
        player: pendingSpot.player,
        enemy: gamePos,
        playerPixel: pendingSpot.playerPixel,
        enemyPixel: { x: pixelX, y: pixelY, imgW, imgH },
        distance: dist,
      };
      setSpots([...spots, newSpot]);
      setPendingSpot(null);
    }
  };

  const handleRemoveSpot = (i) => {
    setSpots(spots.filter((_, idx) => idx !== i));
  };

  const handleRenameSpot = (i, newName) => {
    const updated = [...spots];
    updated[i] = { ...updated[i], name: newName };
    setSpots(updated);
  };

  return (
    <>
      <div className="sidebar">
        <h1>Valorant Spot Editor</h1>

        <select value={selectedMapUuid} onChange={e => { setSelectedMapUuid(e.target.value); setSpots([]); }}>
          <option value="">-- Select Map --</option>
          {maps.map(m => (
            <option key={m.uuid} value={m.uuid}>{m.displayName}</option>
          ))}
        </select>

        {selectedMap && (
          <>
            <div style={{ display: 'flex', gap: 8 }}>
              <button
                onClick={() => { setPlacingMode('player'); setPendingSpot(null); }}
                disabled={placingMode === 'player'}
                style={{ flex: 1, background: placingMode === 'player' ? '#00ff88' : '#ff4655', color: '#111' }}
              >
                {placingMode === 'player' ? 'Placing Player...' : 'Place Player (P)'}
              </button>
              <button
                onClick={() => setPlacingMode(placingMode ? null : 'enemy')}
                style={{
                  flex: 1,
                  background: placingMode === 'enemy' ? '#ff8800' : '#533483',
                  color: '#fff'
                }}
              >
                {placingMode === 'enemy' ? 'Placing Enemy...' : 'Place Enemy (E)'}
              </button>
            </div>
            {placingMode && (
              <button onClick={() => { setPlacingMode(null); setPendingSpot(null); }}>
                Cancel Placement
              </button>
            )}
          </>
        )}

        <h3 style={{ fontSize: 14, marginTop: 8 }}>Spots</h3>
        <SpotList spots={spots} onRemove={handleRemoveSpot} onRename={handleRenameSpot} />

        <ExportPanel spots={spots} mapData={selectedMap} />
      </div>

      <div className="main">
        <Minimap
          ref={minimapRef}
          mapData={selectedMap}
          spots={spots}
          onAddSpot={handleAddSpot}
          placingMode={placingMode}
          setPlacingMode={setPlacingMode}
        />
      </div>
    </>
  );
}
