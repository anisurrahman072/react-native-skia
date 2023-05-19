import { ValueApi } from "../../values";
import { SpringConfig, TimingConfig } from "../types";

/**
 * @description Returns a cached jsContext function for a spring with duration
 * @param mass The mass of the spring
 * @param stiffness The stiffness of the spring
 * @param damping Spring damping
 * @param velocity The initial velocity
 */
export const createSpringEasing = (config: SpringConfig): TimingConfig => {
  return ValueApi.createSpringEasing(config);
};