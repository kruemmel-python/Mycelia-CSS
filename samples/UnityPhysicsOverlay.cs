using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using UnityEngine;
using I18nTokenEngine;

namespace Mycelia.Unity
{
    [RequireComponent(typeof(ComputeShader))]
    public class UnityPhysicsOverlay : MonoBehaviour
    {
        [Header("Catalog")]
        [Tooltip("Relative path to tailwind_style_catalog.i18n (project root).")]
        [SerializeField]
        private string catalogPath = "tailwind_style_catalog.i18n";

        [Header("Style Token")]
        [Tooltip("Style token that drives your compute shader.")]
        [SerializeField]
        private string styleToken = "style_cube-heavy";

        [Header("Compute Shader Bindings")]
        [SerializeField]
        private ComputeShader physicsShader;

        [SerializeField]
        private int kernelIndex = 0;

        private I18n engine;
        private FileSystemWatcher watcher;
        private int refreshPending;

        private void Awake()
        {
            engine = new I18n();
            LoadCatalog();
            SetupWatcher();
        }

        private void Update()
        {
            if (Interlocked.Exchange(ref refreshPending, 0) == 1)
            {
                RefreshShaderValues();
            }
        }

        private void OnDestroy()
        {
            watcher?.Dispose();
            engine?.Dispose();
        }

        private void LoadCatalog()
        {
            string root = Path.IsPathRooted(catalogPath) ? catalogPath : Path.Combine(Application.dataPath, "..", catalogPath);
            engine.LoadFile(root);
            RefreshShaderValues();
        }

        private void SetupWatcher()
        {
            string normalized = Path.GetFullPath(Path.Combine(Application.dataPath, "..", catalogPath));
            string directory = Path.GetDirectoryName(normalized);
            string filename = Path.GetFileName(normalized);

            if (string.IsNullOrEmpty(directory) || string.IsNullOrEmpty(filename))
            {
                return;
            }

            watcher = new FileSystemWatcher(directory, filename)
            {
                NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size
            };
            watcher.Changed += OnCatalogChanged;
            watcher.EnableRaisingEvents = true;
        }

        private void OnCatalogChanged(object sender, FileSystemEventArgs e)
        {
            // Avoid multiple events flooding the UI thread.
            Interlocked.Exchange(ref refreshPending, 1);
            ApplyThreadSafely(() => LoadCatalog());
        }

        private void RefreshShaderValues()
        {
            if (physicsShader == null) return;

            if (engine.TryGetNativeStyle(styleToken, out var native))
            {
                physicsShader.SetFloat("_GlobalMass", native.Mass);
                physicsShader.SetFloat("_GlobalFriction", native.Friction);
                physicsShader.SetFloat("_GlobalSpacing", native.Spacing);
                physicsShader.SetFloat("_GlobalRestitution", native.Restitution);
            }
        }

        private void ApplyThreadSafely(Action callback)
        {
            if (callback == null) return;
            UnityMainThreadDispatcher.Enqueue(callback);
        }
    }
}

/// <summary>
/// Simple dispatcher for Unity Main Thread callbacks (required for FileSystemWatcher).
/// </summary>
public static class UnityMainThreadDispatcher
{
    private static readonly Queue<Action> queue = new Queue<Action>();

    public static void Enqueue(Action action)
    {
        if (action == null) return;
        lock (queue)
        {
            queue.Enqueue(action);
        }
    }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    public static void Initialize()
    {
        if (Application.isPlaying)
        {
            var go = new GameObject("UnityMainThreadDispatcher");
            go.hideFlags = HideFlags.HideAndDontSave;
            go.AddComponent<DispatcherBehaviour>();
        }
    }

    private class DispatcherBehaviour : MonoBehaviour
    {
        private void Update()
        {
            while (true)
            {
                Action action = null;
                lock (queue)
                {
                    if (queue.Count > 0)
                    {
                        action = queue.Dequeue();
                    }
                }
                if (action == null) break;
                action();
            }
        }
    }
}
