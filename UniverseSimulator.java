import javax.swing.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.text.DecimalFormat;

public class UniverseSimulator extends JFrame {
    static {
        System.loadLibrary("universe_physics");
    }

    public native void initSimulation(long seed);
    public native void stepSimulation(double dtYears);
    public native void resetSimulation(long seed);
    public native void setTimeScale(double multiplier);
    public native void setPaused(boolean paused);
    public native double getUniverseAge();
    public native int getParticleCount();
    public native int getStarCount();
    public native int getGalaxyCount();
    public native double getScaleFactor();
    public native double getHubbleConstant();
    public native int getEpoch();
    public native double[] getParticleData();
    public native double[] getUniverseParams();
    public native double[] getPhotonData();

    public native void initRenderer(int width, int height);
    public native void resizeRenderer(int width, int height);
    public native void setCameraOrbit(double theta, double phi, double radius);
    public native void setRenderMode(int mode);
    public native void renderFrame(int[] pixelBuffer, double dt);

    private UniversePanel universePanel;
    private JSlider speedSlider;
    private JLabel timeLabel, infoLabel, epochLabel, scaleLabel;
    private JButton playPauseBtn, resetBtn;
    private JComboBox<String> viewModeCombo;
    private Timer timer;
    private boolean playing = true;
    private long startTime;
    private double currentAge = 0;
    private DecimalFormat df = new DecimalFormat("#.###");
    private DecimalFormat sciFormat = new DecimalFormat("0.000E0");

    private static final String[] EPOCH_NAMES = {
        "Planck Epoch", "Inflation", "Quark Epoch",
        "Hadron/Lepton Epoch", "Recombination",
        "Dark Ages", "Galaxy Formation",
        "Star Formation Era", "Present Day", "Future"
    };

    private static final Color[] EPOCH_COLORS = {
        new Color(255, 200, 255), new Color(255, 220, 180), new Color(200, 150, 255),
        new Color(150, 200, 255), new Color(255, 200, 150),
        new Color(50, 50, 80), new Color(100, 150, 255),
        new Color(200, 255, 200), new Color(180, 220, 255), new Color(255, 100, 100)
    };

    public UniverseSimulator() {
        setTitle("Universe Simulator - 3D Big Bang to Present");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        setPreferredSize(new Dimension(1280, 800));

        universePanel = new UniversePanel();
        add(universePanel, BorderLayout.CENTER);
        add(createControlPanel(), BorderLayout.SOUTH);

        pack();
        setLocationRelativeTo(null);

        KeyStroke spaceKey = KeyStroke.getKeyStroke(KeyEvent.VK_SPACE, 0);
        KeyStroke rKey = KeyStroke.getKeyStroke(KeyEvent.VK_R, 0);
        ActionMap am = getRootPane().getActionMap();
        InputMap im = getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW);
        im.put(spaceKey, "togglePlay");
        im.put(rKey, "reset");
        am.put("togglePlay", new AbstractAction() { public void actionPerformed(ActionEvent e) { togglePlayPause(); } });
        am.put("reset", new AbstractAction() { public void actionPerformed(ActionEvent e) { reset(); } });

        startTime = System.currentTimeMillis();
        initSimulation(System.currentTimeMillis());

        timer = new Timer(16, e -> tick());
        timer.start();
    }

    private JPanel createControlPanel() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.setBackground(new Color(20, 20, 30));
        panel.setBorder(BorderFactory.createEmptyBorder(5, 10, 5, 10));

        JPanel infoPanel = new JPanel(new FlowLayout(FlowLayout.LEFT, 15, 2));
        infoPanel.setBackground(new Color(20, 20, 30));

        timeLabel = new JLabel("Time: 0 s");
        timeLabel.setForeground(Color.WHITE);
        timeLabel.setFont(new Font("Monospaced", Font.BOLD, 13));

        epochLabel = new JLabel("Epoch: Planck");
        epochLabel.setForeground(new Color(255, 200, 255));
        epochLabel.setFont(new Font("Monospaced", Font.BOLD, 13));

        infoLabel = new JLabel("Particles: 0 | Stars: 0 | Galaxies: 0");
        infoLabel.setForeground(Color.LIGHT_GRAY);
        infoLabel.setFont(new Font("Monospaced", Font.PLAIN, 12));

        scaleLabel = new JLabel("3D View");
        scaleLabel.setForeground(Color.LIGHT_GRAY);
        scaleLabel.setFont(new Font("Monospaced", Font.PLAIN, 12));

        infoPanel.add(timeLabel);
        infoPanel.add(epochLabel);
        infoPanel.add(infoLabel);
        infoPanel.add(scaleLabel);

        JPanel ctrlPanel = new JPanel(new BorderLayout(10, 0));
        ctrlPanel.setBackground(new Color(20, 20, 30));

        JPanel leftCtrl = new JPanel(new FlowLayout(FlowLayout.LEFT, 5, 2));
        leftCtrl.setBackground(new Color(20, 20, 30));

        playPauseBtn = new JButton("\u23F8");
        playPauseBtn.setFont(new Font("Monospaced", Font.PLAIN, 18));
        playPauseBtn.setBackground(new Color(60, 60, 80));
        playPauseBtn.setForeground(Color.WHITE);
        playPauseBtn.setFocusPainted(false);
        playPauseBtn.addActionListener(e -> togglePlayPause());

        resetBtn = new JButton("\u23F9");
        resetBtn.setFont(new Font("Monospaced", Font.PLAIN, 18));
        resetBtn.setBackground(new Color(60, 60, 80));
        resetBtn.setForeground(Color.WHITE);
        resetBtn.setFocusPainted(false);
        resetBtn.addActionListener(e -> reset());

        viewModeCombo = new JComboBox<>(new String[]{
            "All Particles", "Stars Only", "Galaxy View", "Dark Matter", "Photons", "Temperature", "Flow Field"
        });
        viewModeCombo.setBackground(new Color(40, 40, 60));
        viewModeCombo.setForeground(Color.WHITE);
        viewModeCombo.setFont(new Font("Monospaced", Font.PLAIN, 11));
        viewModeCombo.addActionListener(e -> {
            int mode = viewModeCombo.getSelectedIndex();
            setRenderMode(mode);
            universePanel.setViewMode(mode);
        });

        leftCtrl.add(playPauseBtn);
        leftCtrl.add(resetBtn);
        leftCtrl.add(new JLabel("  "));
        leftCtrl.add(viewModeCombo);

        JPanel sliderPanel = new JPanel(new BorderLayout(5, 0));
        sliderPanel.setBackground(new Color(20, 20, 30));

        JLabel speedLabel = new JLabel("Speed:");
        speedLabel.setForeground(Color.WHITE);
        speedLabel.setFont(new Font("Monospaced", Font.PLAIN, 12));

        speedSlider = new JSlider(JSlider.HORIZONTAL, -6, 12, 0);
        speedSlider.setBackground(new Color(20, 20, 30));
        speedSlider.setForeground(Color.WHITE);
        speedSlider.setPreferredSize(new Dimension(250, 30));
        speedSlider.setMajorTickSpacing(3);
        speedSlider.setMinorTickSpacing(1);
        speedSlider.setSnapToTicks(false);

        JLabel speedValue = new JLabel("1.0x");
        speedValue.setForeground(new Color(100, 200, 255));
        speedValue.setFont(new Font("Monospaced", Font.BOLD, 12));
        speedValue.setPreferredSize(new Dimension(80, 20));

        speedSlider.addChangeListener(e -> {
            int val = speedSlider.getValue();
            double multiplier = Math.pow(10, val / 3.0);
            speedValue.setText(formatMultiplier(multiplier));
            setTimeScale(multiplier);
        });

        sliderPanel.add(speedLabel, BorderLayout.WEST);
        sliderPanel.add(speedSlider, BorderLayout.CENTER);
        sliderPanel.add(speedValue, BorderLayout.EAST);

        JPanel rightCtrl = new JPanel(new FlowLayout(FlowLayout.RIGHT, 5, 2));
        rightCtrl.setBackground(new Color(20, 20, 30));

        JLabel zoomLabel = new JLabel("3D Orbit: Drag | Zoom: Scroll");
        zoomLabel.setForeground(Color.LIGHT_GRAY);
        zoomLabel.setFont(new Font("Monospaced", Font.PLAIN, 11));
        rightCtrl.add(zoomLabel);

        ctrlPanel.add(leftCtrl, BorderLayout.WEST);
        ctrlPanel.add(sliderPanel, BorderLayout.CENTER);
        ctrlPanel.add(rightCtrl, BorderLayout.EAST);

        panel.add(infoPanel, BorderLayout.NORTH);
        panel.add(ctrlPanel, BorderLayout.SOUTH);

        return panel;
    }

    private String formatMultiplier(double m) {
        if (m < 0.001) return sciFormat.format(m) + "x";
        if (m < 1) return df.format(m) + "x";
        if (m < 10000) return df.format(m) + "x";
        return sciFormat.format(m) + "x";
    }

    private void togglePlayPause() {
        playing = !playing;
        setPaused(!playing);
        playPauseBtn.setText(playing ? "\u23F8" : "\u25B6");
    }

    private void reset() {
        currentAge = 0;
        initSimulation(System.currentTimeMillis());
        playing = true;
        setPaused(false);
        playPauseBtn.setText("\u23F8");
        universePanel.resetView();
    }

    private void tick() {
        if (!playing) return;

        int sliderVal = speedSlider.getValue();
        double multiplier = Math.pow(10, sliderVal / 3.0);
        double baseDtYears = 1000.0;
        double dtYears = baseDtYears * multiplier;

        stepSimulation(dtYears);
        currentAge = getUniverseAge();

        updateDisplay();
        universePanel.repaint();
    }

    private void updateDisplay() {
        double age = currentAge;
        String timeStr;
        if (age < 1) {
            timeStr = String.format("Time: %.4f s", age * YEAR_SECONDS);
        } else if (age < 3.15576e7) {
            timeStr = String.format("Time: %.2f years", age);
        } else if (age < 1e9) {
            timeStr = String.format("Time: %.2f Myr", age / 1e6);
        } else {
            timeStr = String.format("Time: %.2f Gyr", age / 1e9);
        }
        timeLabel.setText(timeStr);

        int epoch = getEpoch();
        if (epoch >= 0 && epoch < EPOCH_NAMES.length) {
            epochLabel.setText("Epoch: " + EPOCH_NAMES[epoch]);
            epochLabel.setForeground(EPOCH_COLORS[epoch]);
        }

        int pCount = getParticleCount();
        int sCount = getStarCount();
        int gCount = getGalaxyCount();
        infoLabel.setText(String.format("Particles: %,d | Stars: %,d | Galaxies: %d",
            pCount, sCount, gCount));
    }

    private static final double LIGHT_YEAR = 9.461e15;
    private static final double YEAR_SECONDS = 3.15576e7;

    class UniversePanel extends JPanel {
        private int viewMode = 0;
        private double orbitTheta = 0.0;
        private double orbitPhi = Math.PI * 0.3;
        private double orbitRadius = 1e20;
        private Point lastMouse;
        private boolean rendererReady = false;
        private int[] pixelBuffer;
        private BufferedImage renderImage;
        private long lastFrameTime = System.nanoTime();

        UniversePanel() {
            setBackground(Color.BLACK);
            setCursor(Cursor.getPredefinedCursor(Cursor.HAND_CURSOR));

            addMouseListener(new MouseAdapter() {
                public void mousePressed(MouseEvent e) {
                    lastMouse = e.getPoint();
                }
                public void mouseReleased(MouseEvent e) {
                    lastMouse = null;
                }
                public void mouseClicked(MouseEvent e) {
                    if (e.getClickCount() == 2) resetView();
                }
            });

            addMouseMotionListener(new MouseMotionAdapter() {
                public void mouseDragged(MouseEvent e) {
                    if (lastMouse != null) {
                        double dx = e.getX() - lastMouse.x;
                        double dy = e.getY() - lastMouse.y;
                        orbitTheta += dx * 0.005;
                        orbitPhi += dy * 0.005;
                        orbitPhi = Math.max(0.05, Math.min(Math.PI - 0.05, orbitPhi));
                        setCameraOrbit(orbitTheta, orbitPhi, orbitRadius);
                        lastMouse = e.getPoint();
                        repaint();
                    }
                }
            });

            addMouseWheelListener(e -> {
                double factor = e.getWheelRotation() < 0 ? 0.8 : 1.25;
                orbitRadius *= factor;
                orbitRadius = Math.max(1e5, Math.min(orbitRadius, 1e30));
                setCameraOrbit(orbitTheta, orbitPhi, orbitRadius);
                repaint();
            });

            addKeyListener(new KeyAdapter() {
                public void keyPressed(KeyEvent e) {
                    if (e.getKeyCode() == KeyEvent.VK_SPACE) togglePlayPause();
                    if (e.getKeyCode() == KeyEvent.VK_R) reset();
                }
            });
            setFocusable(true);
        }

        void setViewMode(int mode) { viewMode = mode; }
        void resetView() {
            orbitTheta = 0.0;
            orbitPhi = Math.PI * 0.3;
            orbitRadius = 1e20;
            setCameraOrbit(orbitTheta, orbitPhi, orbitRadius);
            repaint();
        }

        private void ensureRenderer() {
            if (!rendererReady) {
                int w = getWidth();
                int h = getHeight();
                if (w > 0 && h > 0) {
                    initRenderer(w, h);
                    setCameraOrbit(orbitTheta, orbitPhi, orbitRadius);
                    rendererReady = true;
                }
            }
        }

        protected void paintComponent(Graphics g) {
            super.paintComponent(g);
            int w = getWidth();
            int h = getHeight();

            ensureRenderer();

            if (rendererReady && w > 0 && h > 0) {
                if (pixelBuffer == null || pixelBuffer.length != w * h) {
                    pixelBuffer = new int[w * h];
                    renderImage = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
                    resizeRenderer(w, h);
                }

                long now = System.nanoTime();
                double dt = (now - lastFrameTime) / 1e9;
                lastFrameTime = now;

                renderFrame(pixelBuffer, Math.min(dt, 0.1));
                renderImage.setRGB(0, 0, w, h, pixelBuffer, 0, w);
                g.drawImage(renderImage, 0, 0, null);
            } else {
                Graphics2D g2 = (Graphics2D)g;
                g2.setColor(Color.BLACK);
                g2.fillRect(0, 0, w, h);
            }

            drawInfoOverlay((Graphics2D)g, w, h);
        }

        private void drawInfoOverlay(Graphics2D g2, int w, int h) {
            double age = currentAge;
            int epoch = getEpoch();

            String ageStr;
            if (age < 1) {
                ageStr = String.format("Age: %.4f s", age * YEAR_SECONDS);
            } else if (age < 3.15576e7) {
                ageStr = String.format("Age: %.2f yr", age);
            } else if (age < 1e9) {
                ageStr = String.format("Age: %.2f Myr", age / 1e6);
            } else {
                ageStr = String.format("Age: %.2f Gyr", age / 1e9);
            }

            String epochStr = epoch >= 0 && epoch < EPOCH_NAMES.length ? EPOCH_NAMES[epoch] : "Unknown";

            g2.setColor(new Color(0, 0, 0, 150));
            g2.fillRect(8, 8, 220, 70);

            g2.setColor(Color.WHITE);
            g2.setFont(new Font("Monospaced", Font.BOLD, 14));
            g2.drawString("UNIVERSE SIMULATION", 16, 26);

            g2.setFont(new Font("Monospaced", Font.PLAIN, 12));
            g2.setColor(new Color(200, 200, 255));
            g2.drawString(ageStr, 16, 44);

            Color eColor = epoch >= 0 && epoch < EPOCH_COLORS.length ? EPOCH_COLORS[epoch] : Color.WHITE;
            g2.setColor(eColor);
            g2.drawString("Epoch: " + epochStr, 16, 62);

            g2.setColor(new Color(0, 0, 0, 150));
            g2.fillRect(8, h - 55, 280, 47);

            g2.setColor(Color.LIGHT_GRAY);
            g2.setFont(new Font("Monospaced", Font.PLAIN, 10));
            double hubble = getHubbleConstant();
            double scale = getScaleFactor();
            g2.drawString(String.format("H\u2080: %.2f km/s/Mpc  a=%.6f  z=%.2f",
                hubble, scale, (1.0/scale - 1.0)), 16, h - 38);
            g2.drawString(String.format("Particles: %,d  |  \u2606 Stars: %d  |  NN: %s",
                getParticleCount(), getStarCount(), "Active"), 16, h - 22);

            g2.setFont(new Font("Monospaced", Font.PLAIN, 9));
            g2.setColor(new Color(150, 150, 150, 100));
            g2.drawString("[Space] Pause/Play  [R] Reset  [Drag] 3D Orbit  [Scroll] Zoom", 16, h - 6);
        }
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            try {
                UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
            } catch (Exception e) {
                try {
                    UIManager.setLookAndFeel(UIManager.getCrossPlatformLookAndFeelClassName());
                } catch (Exception ex) {}
            }
            new UniverseSimulator().setVisible(true);
        });
    }
}
