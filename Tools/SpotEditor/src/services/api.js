const API_BASE = 'https://valorant-api.com/v1';

export async function fetchMaps() {
  try {
    const res = await fetch(`${API_BASE}/maps`);
    const data = await res.json();
    return data.data
      .filter(m => m.displayIcon && m.xMultiplier)
      .map(m => ({
        uuid: m.uuid,
        displayName: m.displayName,
        displayIcon: m.displayIcon,
        xMultiplier: m.xMultiplier,
        xScalarToAdd: m.xScalarToAdd,
        yMultiplier: m.yMultiplier,
        yScalarToAdd: m.yScalarToAdd,
      }));
  } catch (e) {
    console.error('Failed to fetch maps:', e);
    return [];
  }
}
