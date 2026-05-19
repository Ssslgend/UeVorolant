import React, { useRef, useEffect, useCallback, forwardRef, useImperativeHandle } from 'react';
import { minimapToGame } from '../services/coordUtils';

const Minimap = forwardRef(function Minimap({ mapData, spots, onAddSpot, placingMode, setPlacingMode }, ref) {
  const imgRef = useRef(null);
  const canvasRef = useRef(null);

  useImperativeHandle(ref, () => ({
    getImgRef: () => imgRef.current,
  }));

  const drawSpots = useCallback(() => {
    const canvas = canvasRef.current;
    const img = imgRef.current;
    if (!canvas || !img) return;

    const rect = img.getBoundingClientRect();
    canvas.width = rect.width;
    canvas.height = rect.height;
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    const scaleX = rect.width / img.naturalWidth;
    const scaleY = rect.height / img.naturalHeight;

    spots.forEach((spot, i) => {
      const pxPx = spot.playerPixel.x * scaleX;
      const pxPy = spot.playerPixel.y * scaleY;
      const exPx = spot.enemyPixel.x * scaleX;
      const exPy = spot.enemyPixel.y * scaleY;

      // Line between P and E
      ctx.beginPath();
      ctx.moveTo(pxPx, pxPy);
      ctx.lineTo(exPx, exPy);
      ctx.strokeStyle = '#ff4655';
      ctx.lineWidth = 2;
      ctx.setLineDash([6, 4]);
      ctx.stroke();
      ctx.setLineDash([]);

      // Player dot (green)
      ctx.beginPath();
      ctx.arc(pxPx, pxPy, 8, 0, Math.PI * 2);
      ctx.fillStyle = '#00ff88';
      ctx.fill();
      ctx.strokeStyle = '#fff';
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.fillStyle = '#fff';
      ctx.font = '10px monospace';
      ctx.fillText('P', pxPx + 12, pxPy + 4);

      // Enemy dot (red)
      ctx.beginPath();
      ctx.arc(exPx, exPy, 8, 0, Math.PI * 2);
      ctx.fillStyle = '#ff4655';
      ctx.fill();
      ctx.strokeStyle = '#fff';
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.fillStyle = '#fff';
      ctx.font = '10px monospace';
      ctx.fillText('E', exPx + 12, exPy + 4);

      // Index label
      ctx.fillStyle = '#fff';
      ctx.font = '11px sans-serif';
      ctx.fillText(`${i + 1}`, pxPx - 4, pxPy - 12);
    });
  }, [spots]);

  useEffect(() => {
    drawSpots();
    const handler = () => drawSpots();
    window.addEventListener('resize', handler);
    return () => window.removeEventListener('resize', handler);
  }, [drawSpots]);

  useEffect(() => {
    drawSpots();
  }, [spots, drawSpots]);

  const handleImageClick = (e) => {
    if (!placingMode) return;

    const img = imgRef.current;
    if (!img) return;

    const rect = img.getBoundingClientRect();
    const pixelX = (e.clientX - rect.left) * (img.naturalWidth / rect.width);
    const pixelY = (e.clientY - rect.top) * (img.naturalHeight / rect.height);

    const gamePos = minimapToGame(pixelX, pixelY, img.naturalWidth, img.naturalHeight, mapData);

    onAddSpot({ pixelX, pixelY, gamePos, imgW: img.naturalWidth, imgH: img.naturalHeight });

    if (placingMode === 'player') {
      setPlacingMode('enemy');
    } else {
      setPlacingMode(null);
    }
  };

  return (
    <div className="minimap-container">
      {mapData ? (
        <>
          <img
            ref={imgRef}
            src={mapData.displayIcon}
            alt={mapData.displayName}
            onClick={handleImageClick}
            style={{ cursor: placingMode ? 'crosshair' : 'default' }}
            draggable={false}
          />
          <canvas ref={canvasRef} style={{ pointerEvents: 'none' }} />
        </>
      ) : (
        <div style={{ padding: 40, color: '#666' }}>Select a map to begin</div>
      )}
    </div>
  );
});

export default Minimap;
