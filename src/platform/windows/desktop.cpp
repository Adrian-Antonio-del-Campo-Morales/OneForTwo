/*
 * OneInTwo — Contexto del escritorio Windows.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Crear y cerrar los recursos nativos de sesión y despachar notificaciones a los componentes de Windows.
 * - Observar bloqueo, fin de sesión y cambios de pantalla; publicar geometría y capacidades reales.
 * - Coordinar ventanas auxiliares, bucle de mensajes y afinidad de hilo que exijan las APIs.
 * - Conectar el contrato común con clipboard.cpp, file_clipboard.cpp e input.cpp de este directorio.
 * - No ejecutar como administrador toda la aplicación ni prometer control del escritorio seguro.
 */

