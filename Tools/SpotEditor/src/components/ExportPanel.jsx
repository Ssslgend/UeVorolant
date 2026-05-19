import React, { useState, useMemo } from 'react';

export default function ExportPanel({ spots, mapData }) {
  const [copied, setCopied] = useState(false);

  const jsonOutput = useMemo(() => {
    const data = {
      version: '1.0',
      spots: spots.map(s => ({
        name: s.name,
        map: mapData?.displayName || '',
        mapUuid: mapData?.uuid || '',
        player: {
          gamePosition: { x: s.player.x, y: s.player.y, z: 0 },
          minimapNormalized: {
            x: s.playerPixel.x / s.playerPixel.imgW,
            y: s.playerPixel.y / s.playerPixel.imgH
          }
        },
        enemy: {
          gamePosition: { x: s.enemy.x, y: s.enemy.y, z: 0 },
          minimapNormalized: {
            x: s.enemyPixel.x / s.enemyPixel.imgW,
            y: s.enemyPixel.y / s.enemyPixel.imgH
          }
        },
        distanceUnits: Math.round(s.distance)
      }))
    };
    return JSON.stringify(data, null, 2);
  }, [spots, mapData]);

  const handleCopy = () => {
    navigator.clipboard.writeText(jsonOutput).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  const handleDownload = () => {
    const blob = new Blob([jsonOutput], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'prefire_spots.json';
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="export-panel">
      <h3 style={{ marginBottom: 8, fontSize: 14 }}>JSON Export ({spots.length} spots)</h3>
      <textarea readOnly value={jsonOutput} />
      <div className="export-actions">
        <button onClick={handleCopy} disabled={spots.length === 0 || copied}>
          {copied ? 'Copied!' : 'Copy JSON'}
        </button>
        <button onClick={handleDownload} disabled={spots.length === 0}>
          Download .json
        </button>
      </div>
    </div>
  );
}
