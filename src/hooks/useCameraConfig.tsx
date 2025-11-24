import { useState, useEffect } from 'react';

export interface CameraConfig {
  ip: string;
  username: string;
  password: string;
  port: number;
  streamPath: string;
  snapshotPath: string;
  streamType: 'mjpeg' | 'snapshot' | 'hls';
}

const DEFAULT_CONFIG: CameraConfig = {
  ip: '192.168.0.17',
  username: 'admin',
  password: 'Crepaldi',
  port: 80,
  streamPath: '/video/mjpeg.cgi',
  snapshotPath: '/snapshot.jpg',
  streamType: 'mjpeg',
};

const STORAGE_KEY = 'aquasys_camera_config';

export function useCameraConfig() {
  const [config, setConfig] = useState<CameraConfig>(() => {
    const stored = localStorage.getItem(STORAGE_KEY);
    return stored ? JSON.parse(stored) : DEFAULT_CONFIG;
  });

  useEffect(() => {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
  }, [config]);

  const getMjpegUrl = () => {
    return `http://${config.ip}:${config.port}${config.streamPath}`;
  };

  const getSnapshotUrl = () => {
    return `http://${config.ip}:${config.port}${config.snapshotPath}`;
  };

  const updateConfig = (updates: Partial<CameraConfig>) => {
    setConfig(prev => ({ ...prev, ...updates }));
  };

  const resetToDefaults = () => {
    setConfig(DEFAULT_CONFIG);
  };

  return {
    config,
    updateConfig,
    resetToDefaults,
    getMjpegUrl,
    getSnapshotUrl,
  };
}
