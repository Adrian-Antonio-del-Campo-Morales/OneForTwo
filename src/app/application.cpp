/*
 * OneInTwo — Composición y ciclo de vida.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Crear y conectar sesión, transporte, almacenamiento, módulos, escritorio e interfaz.
 * - Mantener una instancia por sesión de usuario; una segunda apertura debe activar la ventana existente.
 * - Distribuir eventos del escritorio: texto al módulo de portapapeles y ofertas de archivos al de archivos.
 * - Conectar el selector de envío directo con archivos y la confirmación de URL con enlaces.
 * - Conectar invitaciones y visor visual con sesión y captura local. El cierre del visor termina su operación.
 * - Conectar formulario, avisos observados y presentación con el módulo de notificaciones.
 * - Aplicar el perfil seleccionado a través de la sesión y comunicar a cada módulo las capacidades retiradas.
 * - Aplicar configuración y propagar desconexión, bloqueo y revocación a los módulos.
 * - Cerrar liberando captura y teclas, cancelando operaciones y preservando estado válido.
 * - Coordinar dependencias explícitas; no introducir un bus genérico ni contener algoritmos de cada módulo.
 */


/*
 * Construir los módulos con dependencias explícitas y seleccionar fábricas de plataforma al compilar. Conectar mensajes autorizados a las entradas del módulo y sus salidas al transporte. Entregar callbacks nativos al ejecutor del módulo y presentación al hilo de UI. Al cerrar, invalidar operaciones antes de destruir sus dependencias.
 */
