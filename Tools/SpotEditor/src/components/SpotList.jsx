import React from 'react';

export default function SpotList({ spots, onRemove, onRename }) {
  if (spots.length === 0) {
    return <div style={{ color: '#666', fontSize: 13 }}>No spots yet. Select a map and click to place.</div>;
  }

  return (
    <div>
      {spots.map((spot, i) => (
        <div className="spot-item" key={i}>
          <div className="spot-header">
            <input
              value={spot.name}
              onChange={(e) => onRename(i, e.target.value)}
              style={{ background: 'transparent', color: '#eee', border: 'none', fontWeight: 'bold', width: '100%' }}
            />
            <span className="remove" onClick={() => onRemove(i)} title="Remove">x</span>
          </div>
          <div className="spot-detail">
            P({spot.player.x.toFixed(0)}, {spot.player.y.toFixed(0)})
            &nbsp;&rarr;&nbsp;
            E({spot.enemy.x.toFixed(0)}, {spot.enemy.y.toFixed(0)})
          </div>
          <div className="spot-detail">
            Distance: {spot.distance.toFixed(0)} units
          </div>
        </div>
      ))}
    </div>
  );
}
