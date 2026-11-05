/*
 * OneInTwo — Composición y ciclo de vida.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Crear y conectar sesión, transporte, almacenamiento, módulos, escritorio e interfaz.
 * - Mantener una instancia por sesión de usuario; una segunda apertura debe activar la ventana existente.
 * - Distribuir eventos del escritorio: texto al módulo de portapapeles y ofertas de archivos al de archivos.
 * - Aplicar configuración y propagar desconexión, bloqueo y revocación a los módulos.
 * - Cerrar liberando captura y teclas, cancelando operaciones y preservando estado válido.
 * - Coordinar dependencias explícitas; no introducir un bus genérico ni contener algoritmos de cada módulo.
 */

